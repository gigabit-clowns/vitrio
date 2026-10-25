// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <vitrio/file_image_scratch_storage.hpp>

#include <file_image_scratch_storage_creation.hpp>
#include <file_transfer_pool_implementation.hpp>

#include "fixtures/image_scratch_storage_contract.hpp"
#include "mock/mock_image_reader.hpp"

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/image_scratch_storage.hpp>
#include <vitrio/span.hpp>
#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include <boost/filesystem/operations.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include <trompeloeil.hpp>

using namespace vitrio;

namespace
{

std::string read_file(const std::string &path)
{
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	return std::string(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()
	);
}

void write_file(const std::string &path, const std::string &text)
{
	std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
	output << text;
}

// What a storage holds, as text.
std::string read_storage(const image_scratch_storage &storage)
{
	std::string text(storage.get_size(), ' ');
	storage.read(0, make_span(reinterpret_cast<byte*>(&text[0]), text.size()));
	return text;
}

void write_storage(image_scratch_storage &storage, const std::string &text)
{
	storage.write(
		0,
		make_span(reinterpret_cast<const byte*>(text.data()), text.size())
	);
}

std::shared_ptr<image_scratch_storage> make_storage(
	const std::string &path,
	std::size_t size
)
{
	return create_file_image_scratch_storage(
		path,
		size,
		*test::make_file_transfer_pool()
	);
}

// Reads that wait until the case finishes them, in the order they came,
// counting how many wait at once.
class held_reads
{
public:
	void hold(std::shared_ptr<completion_notifier> done)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_waiting.push_back(std::move(done));
		++m_started;
		m_most = std::max(m_most, m_waiting.size());
		m_changed.notify_all();
	}

	// Whether as many reads wait within a time.
	bool wait_for(std::size_t count)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_changed.wait_for(
			lock,
			std::chrono::seconds(10),
			[this, count] { return m_waiting.size() >= count; }
		);
	}

	// Finishes the read that waited longest, once there is one, or tells
	// that none came within a time.
	bool finish_next(std::exception_ptr error = nullptr)
	{
		if (!wait_for(1))
		{
			return false;
		}

		std::shared_ptr<completion_notifier> done;
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			done = std::move(m_waiting.front());
			m_waiting.pop_front();
		}
		done->notify_done(std::move(error));
		return true;
	}

	std::size_t get_started()
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_started;
	}

	std::size_t get_most()
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		return m_most;
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_changed;
	std::deque<std::shared_ptr<completion_notifier>> m_waiting;
	std::size_t m_started = 0;
	std::size_t m_most = 0;
};

} // anonymous namespace

TEST_CASE(
	"a file image scratch storage keeps the contract of a scratch storage",
	"[file_image_scratch_storage]"
)
{
	const scoped_path path("file_scratch_storage_contract.scratch");

	check_image_scratch_storage_contract(
		"file_scratch_storage_contract",
		[&](std::size_t size) { return make_storage(path.get(), size); }
	);
}

TEST_CASE(
	"a file image scratch storage that loads an image a piece keeps the "
	"contract of a scratch storage",
	"[file_image_scratch_storage]"
)
{
	const scoped_path path("file_scratch_storage_pieces.scratch");
	const auto pool = test::make_file_transfer_pool();

	// The images of the contract are of 2 by 2 float32: 16 bytes.
	check_image_scratch_storage_contract(
		"file_scratch_storage_pieces",
		[&](std::size_t size)
		{
			return create_file_image_scratch_storage(
				path.get(),
				size,
				pool->get_implementation().get_executor(),
				16,
				32
			);
		}
	);
}

TEST_CASE(
	"a file image scratch storage stages as much of its loads at once as it "
	"is given",
	"[file_image_scratch_storage]"
)
{
	const scoped_path path("file_scratch_storage_admission.scratch");
	const auto pool = test::make_file_transfer_pool();
	const auto storage = create_file_image_scratch_storage(
		path.get(),
		4096,
		pool->get_implementation().get_executor(),
		16,
		32
	);

	// Six images of 2 by 2 float32, a piece each, two pieces at once.
	const array_descriptor values(
		{6, 2, 2},
		{4, 2, 1},
		0,
		numerical_type::float32
	);
	held_reads reads;
	const auto file = std::make_shared<mock_image_reader>();
	ALLOW_CALL(
		*file,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_SIDE_EFFECT( reads.hold(_3) );

	const auto done = std::make_shared<counting_completion>(1);
	storage->load_async(
		file,
		values,
		images_into_slots({0, 1, 2, 3, 4, 5}, {0, 1, 2, 3, 4, 5}),
		done
	);

	// Two pieces start, and the others wait for them.
	REQUIRE( reads.wait_for(2) );
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	CHECK( reads.get_started() == 2 );

	SECTION( "and starts each that waited as an earlier one finishes" )
	{
		for (int piece = 0; piece < 6; ++piece)
		{
			REQUIRE( reads.finish_next() );
		}

		CHECK_NOTHROW( done->get() );
	}

	SECTION( "and fails the load with a piece that fails, once every piece "
		"has finished" )
	{
		const auto failure =
			std::make_exception_ptr(std::runtime_error("read"));
		REQUIRE( reads.finish_next() );
		REQUIRE( reads.finish_next(failure) );
		for (int piece = 2; piece < 6; ++piece)
		{
			REQUIRE( reads.finish_next() );
		}

		CHECK_THROWS_AS( done->get(), std::runtime_error );
	}

	CHECK( reads.get_started() == 6 );
	CHECK( reads.get_most() == 2 );
}

TEST_CASE(
	"create_file_image_scratch_storage lays out a file of the size it is "
	"given",
	"[file_image_scratch_storage]"
)
{
	const scoped_path path("file_scratch_storage_size.scratch");

	const auto storage = make_storage(path.get(), 4096);

	CHECK( storage->get_size() == 4096 );
	CHECK( boost::filesystem::file_size(path.get()) == 4096 );
}

TEST_CASE(
	"what is written to a file image scratch storage is in its file",
	"[file_image_scratch_storage]"
)
{
	const scoped_path path("file_scratch_storage_write.scratch");
	const std::string text = "what a scratch holds";
	auto storage = make_storage(path.get(), text.size());
	write_storage(*storage, text);

	SECTION( "while the storage is alive" )
	{
		CHECK( read_file(path.get()) == text );
	}

	SECTION( "and after it is destroyed, since the file is not removed" )
	{
		storage.reset();

		REQUIRE( boost::filesystem::exists(path.get()) );
		CHECK( read_file(path.get()) == text );
	}
}

TEST_CASE(
	"create_file_image_scratch_storage keeps what a file already at its path "
	"holds",
	"[file_image_scratch_storage]"
)
{
	const scoped_path path("file_scratch_storage_keep.scratch");
	const std::string text = "left by another run";
	write_file(path.get(), text);

	SECTION( "all of it when the size is that of the file" )
	{
		const auto storage = make_storage(path.get(), text.size());

		CHECK( read_storage(*storage) == text );
	}

	SECTION( "the start of it when the size is smaller" )
	{
		const auto storage = make_storage(path.get(), 4);

		CHECK( boost::filesystem::file_size(path.get()) == 4 );
		CHECK( read_storage(*storage) == text.substr(0, 4) );
	}

	SECTION( "followed by more room when the size is larger" )
	{
		const auto storage = make_storage(path.get(), 64);

		CHECK( boost::filesystem::file_size(path.get()) == 64 );
		CHECK( read_storage(*storage).substr(0, text.size()) == text );
	}
}

TEST_CASE(
	"create_file_image_scratch_storage refuses a file it can not lay out",
	"[file_image_scratch_storage]"
)
{
	SECTION( "a file of no bytes" )
	{
		const scoped_path path("file_scratch_storage_empty.scratch");

		REQUIRE_THROWS_AS(make_storage(path.get(), 0), image_file_error);
		CHECK_FALSE( boost::filesystem::exists(path.get()) );
	}

	SECTION( "a file in a directory that does not exist" )
	{
		const scoped_path path("file_scratch_storage_absent/values.scratch");

		REQUIRE_THROWS_MATCHES(
			make_storage(path.get(), 64),
			image_file_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(path.get() + ": ")
			)
		);
	}
}

TEST_CASE(
	"file image scratch storages over the same file share its contents",
	"[file_image_scratch_storage]"
)
{
	const scoped_path path("file_scratch_storage_shared.scratch");
	const std::string first_text = "from the first";
	const std::string second_text = "by the second";
	const auto first = make_storage(path.get(), 32);
	write_storage(*first, first_text);

	const auto second = make_storage(path.get(), 32);

	SECTION( "what one wrote before the other was created" )
	{
		CHECK(
			read_storage(*second).substr(0, first_text.size()) == first_text
		);
	}

	SECTION( "and what one writes afterwards" )
	{
		write_storage(*second, second_text);

		CHECK(
			read_storage(*first).substr(0, second_text.size()) == second_text
		);
	}
}
