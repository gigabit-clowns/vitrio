// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/zarr/zarr_read_operation.hpp>

#include <formats/zarr/zarr_array_layout.hpp>
#include <formats/zarr/zarr_node.hpp>

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <vitrio/tests/assets.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <system/scattered_file_read.hpp>

#include "../../mock/mock_file_transfer_scope.hpp"
#include "zarr_test_store.hpp"

#include <catch2/interfaces/catch_interfaces_capture.hpp>

#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;
using namespace vitrio::zarr;
using trompeloeil::_;

namespace
{

using extents = std::vector<std::size_t>;

const std::uint64_t open_step = 0;
const std::uint64_t decode_step = 1;
const std::uint64_t whole_step = 2;
const std::uint64_t read_index_step = 3;
const std::uint64_t read_chunk_step = 4;

std::uint64_t tag_of(std::size_t unit, std::uint64_t step)
{
	return static_cast<std::uint64_t>(unit) << 3 | step;
}

// A store of 4 by 4 bytes in chunks of 2 by 2, stored as they are: chunk
// (r, c) holds 10 * (2 r + c) + its position in it, which is the value of
// each byte.
class read_operation_fixture
{
public:
	explicit read_operation_fixture(const char *compressor = "null")
		: m_directory("test_zarr_read_operation_" + std::to_string(
			std::hash<std::string>()(
				Catch::getResultCapture().getCurrentTestName())) + ".zarr")
		, m_destination(make_host_array<std::uint8_t>(
			{4, 4}, numerical_type::uint8, 255))
	{
		write_store_file(m_directory.get(), ".zarray",
			std::string(R"({"zarr_format": 2, "shape": [4, 4],
				"chunks": [2, 2], "dtype": "|u1", "order": "C",
				"fill_value": 99, "filters": null, "compressor": )") +
				compressor + "}");
		for (std::size_t chunk = 0; chunk < 4; ++chunk)
		{
			std::string bytes;
			for (std::size_t i = 0; i < 4; ++i)
			{
				bytes.push_back(static_cast<char>(10 * chunk + i));
			}
			write_store_file(m_directory.get(), key_of(chunk), bytes);
		}
	}

	static std::string key_of(std::size_t chunk)
	{
		return std::to_string(chunk / 2) + "." + std::to_string(chunk % 2);
	}

	std::string path_of(std::size_t chunk) const
	{
		return join_zarr_path(m_directory.get(), key_of(chunk));
	}

	std::unique_ptr<zarr_read_operation> make_operation(
		bool asynchronous,
		std::size_t chunks_in_flight = 64
	)
	{
		const auto layout = std::make_shared<const zarr_array_layout>(
			read_zarr_node(m_directory.get()));
		const auto regions = whole_of({4, 4});
		image_chunk_regions chunks(
			regions,
			layout->get_shape(),
			layout->get_chunk_layout().get_extents()
		);

		return std::make_unique<zarr_read_operation>(
			m_directory.get(), layout, m_destination.share(),
			std::move(chunks), asynchronous, 1UL << 20, chunks_in_flight);
	}

	// Takes every task the operation runs, by its tag.
	std::unique_ptr<trompeloeil::expectation> take_tasks()
	{
		return NAMED_ALLOW_CALL(m_scope, run(_, _))
			.LR_SIDE_EFFECT( m_tasks[_2] = std::move(_1) );
	}

	// Takes every read the operation queues, by its tag.
	std::unique_ptr<trompeloeil::expectation> take_reads()
	{
		return NAMED_ALLOW_CALL(m_scope, queue_read(_, 0u, _, _))
			.LR_SIDE_EFFECT(
				REQUIRE( _3.size() == 1 );
				m_reads.emplace(_4, _3[0])
			);
	}

	// Runs a task the operation ran, and hands it its outcome.
	void finish(zarr_read_operation &operation, std::uint64_t tag)
	{
		auto work = std::move(m_tasks.at(tag));
		m_tasks.erase(tag);
		work->run();
		operation.on_task(tag, nullptr, m_scope);
	}

	// Lands the bytes of a chunk in the buffer of its read, as the kernel
	// would, and hands the operation its completion.
	void land(zarr_read_operation &operation, std::size_t chunk)
	{
		const auto tag = tag_of(chunk, read_chunk_step);
		const auto buffer = m_reads.at(tag);
		m_reads.erase(tag);
		for (std::size_t i = 0; i < buffer.get_size(); ++i)
		{
			static_cast<byte*>(buffer.get_address())[i] =
				static_cast<byte>(10 * chunk + i);
		}
		operation.on_transfer(
			file_transfer_completion(tag, buffer.get_size()), m_scope);
	}

	std::vector<std::uint8_t> values() const
	{
		return get_values<std::uint8_t>(m_destination);
	}

	// What the array holds once every chunk has landed, or with one of them
	// holding the fill value instead.
	static std::vector<std::uint8_t> expected(int filled = -1)
	{
		std::vector<std::uint8_t> values(16);
		for (std::size_t row = 0; row < 4; ++row)
		{
			for (std::size_t column = 0; column < 4; ++column)
			{
				const auto chunk = (row / 2) * 2 + column / 2;
				const auto inside = (row % 2) * 2 + column % 2;
				values[row * 4 + column] = static_cast<int>(chunk) == filled
					? 99
					: static_cast<std::uint8_t>(10 * chunk + inside);
			}
		}

		return values;
	}

	scoped_directory m_directory;
	array m_destination;
	mock_file_transfer_scope m_scope;
	std::map<std::uint64_t, std::unique_ptr<task>> m_tasks;
	std::map<std::uint64_t, memory_range> m_reads;
};

} // anonymous namespace

TEST_CASE( "zarr_read_operation opens, reads and decodes every chunk",
	"[zarr_read_operation]" )
{
	read_operation_fixture fixture;
	const auto tasks = fixture.take_tasks();
	const auto reads = fixture.take_reads();
	auto operation = fixture.make_operation(true);

	operation->start(fixture.m_scope);
	REQUIRE( fixture.m_tasks.size() == 4 );

	for (std::size_t chunk = 0; chunk < 4; ++chunk)
	{
		fixture.finish(*operation, tag_of(chunk, open_step));
	}
	REQUIRE( fixture.m_reads.size() == 4 );
	REQUIRE( fixture.m_reads.at(tag_of(2, read_chunk_step)).get_size() ==
		4 );

	for (std::size_t chunk = 0; chunk < 4; ++chunk)
	{
		fixture.land(*operation, chunk);
		fixture.finish(*operation, tag_of(chunk, decode_step));
	}

	REQUIRE( fixture.m_tasks.empty() );
	REQUIRE( fixture.values() == read_operation_fixture::expected() );
}

TEST_CASE( "zarr_read_operation fills a chunk that is not stored",
	"[zarr_read_operation]" )
{
	read_operation_fixture fixture;
	boost::filesystem::remove(fixture.path_of(1));
	const auto tasks = fixture.take_tasks();
	const auto reads = fixture.take_reads();
	auto operation = fixture.make_operation(true);

	operation->start(fixture.m_scope);
	for (std::size_t chunk = 0; chunk < 4; ++chunk)
	{
		fixture.finish(*operation, tag_of(chunk, open_step));
	}

	// The missing chunk goes straight to its decoding, with nothing read.
	REQUIRE( fixture.m_reads.count(tag_of(1, read_chunk_step)) == 0 );
	REQUIRE( fixture.m_tasks.count(tag_of(1, decode_step)) == 1 );

	for (const std::size_t chunk : {0, 2, 3})
	{
		fixture.land(*operation, chunk);
	}
	for (std::size_t chunk = 0; chunk < 4; ++chunk)
	{
		fixture.finish(*operation, tag_of(chunk, decode_step));
	}

	REQUIRE( fixture.values() == read_operation_fixture::expected(1) );
}

TEST_CASE( "zarr_read_operation reports what fails, naming the chunk",
	"[zarr_read_operation]" )
{
	SECTION( "a read the system failed" )
	{
		read_operation_fixture fixture;
		const auto tasks = fixture.take_tasks();
		const auto reads = fixture.take_reads();
		auto operation = fixture.make_operation(true);
		operation->start(fixture.m_scope);
		fixture.finish(*operation, tag_of(0, open_step));

		REQUIRE_THROWS_MATCHES(
			operation->on_transfer(
				file_transfer_completion(
					tag_of(0, read_chunk_step),
					std::make_error_code(std::errc::io_error)),
				fixture.m_scope),
			image_file_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(fixture.path_of(0) + ": "))
		);
	}

	SECTION( "a read that ends short" )
	{
		read_operation_fixture fixture;
		const auto tasks = fixture.take_tasks();
		const auto reads = fixture.take_reads();
		auto operation = fixture.make_operation(true);
		operation->start(fixture.m_scope);
		fixture.finish(*operation, tag_of(0, open_step));

		REQUIRE_THROWS_AS(
			operation->on_transfer(
				file_transfer_completion(
					tag_of(0, read_chunk_step), std::size_t(2)),
				fixture.m_scope),
			image_file_format_error
		);
	}

	SECTION( "a chunk its codec refuses" )
	{
		read_operation_fixture fixture(R"({"id": "zlib"})");
		const auto tasks = fixture.take_tasks();
		const auto reads = fixture.take_reads();
		auto operation = fixture.make_operation(true);
		operation->start(fixture.m_scope);
		fixture.finish(*operation, tag_of(0, open_step));
		fixture.land(*operation, 0);

		auto decoding = std::move(fixture.m_tasks.at(tag_of(0, decode_step)));
		REQUIRE_THROWS_MATCHES(
			decoding->run(),
			image_file_format_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(fixture.path_of(0) + ": "))
		);
	}

	SECTION( "a stored chunk of another size than its values" )
	{
		read_operation_fixture fixture;
		write_store_file(fixture.m_directory.get(), "0.0", "abc");
		const auto tasks = fixture.take_tasks();
		const auto reads = fixture.take_reads();
		auto operation = fixture.make_operation(false);
		operation->start(fixture.m_scope);

		auto whole = std::move(fixture.m_tasks.at(tag_of(0, whole_step)));
		REQUIRE_THROWS_AS(whole->run(), image_file_format_error);
	}

	SECTION( "a file far larger than any encoding of its chunk" )
	{
		read_operation_fixture fixture;
		write_store_file(fixture.m_directory.get(), "0.0",
			std::string(1UL << 17, 'x'));
		const auto tasks = fixture.take_tasks();
		auto operation = fixture.make_operation(true);
		operation->start(fixture.m_scope);

		auto opening = std::move(fixture.m_tasks.at(tag_of(0, open_step)));
		REQUIRE_THROWS_AS(opening->run(), image_file_format_error);
	}

	SECTION( "a task that failed" )
	{
		read_operation_fixture fixture;
		const auto tasks = fixture.take_tasks();
		auto operation = fixture.make_operation(true);
		operation->start(fixture.m_scope);

		REQUIRE_THROWS_AS(
			operation->on_task(
				tag_of(0, open_step),
				std::make_exception_ptr(std::runtime_error("open")),
				fixture.m_scope),
			std::runtime_error
		);
	}
}

TEST_CASE( "zarr_read_operation holds no more chunks than it may",
	"[zarr_read_operation]" )
{
	read_operation_fixture fixture;
	const auto tasks = fixture.take_tasks();
	const auto reads = fixture.take_reads();
	auto operation = fixture.make_operation(true, 2);

	operation->start(fixture.m_scope);
	REQUIRE( fixture.m_tasks.size() == 2 );

	fixture.finish(*operation, tag_of(0, open_step));
	fixture.land(*operation, 0);
	REQUIRE( fixture.m_tasks.count(tag_of(2, open_step)) == 0 );

	// A chunk done makes room for the next.
	fixture.finish(*operation, tag_of(0, decode_step));
	REQUIRE( fixture.m_tasks.count(tag_of(2, open_step)) == 1 );
}

TEST_CASE( "zarr_read_operation reads each chunk in one task without "
	"asynchronous ports", "[zarr_read_operation]" )
{
	read_operation_fixture fixture;
	boost::filesystem::remove(fixture.path_of(3));
	const auto tasks = fixture.take_tasks();
	FORBID_CALL(fixture.m_scope, queue_read(_, _, _, _));
	auto operation = fixture.make_operation(false);

	operation->start(fixture.m_scope);
	REQUIRE( fixture.m_tasks.size() == 4 );
	for (std::size_t chunk = 0; chunk < 4; ++chunk)
	{
		fixture.finish(*operation, tag_of(chunk, whole_step));
	}

	REQUIRE( fixture.m_tasks.empty() );
	REQUIRE( fixture.values() == read_operation_fixture::expected(3) );
}

namespace
{

// A sharded store of the assets, read with every read landed from the file
// it was queued for, as the kernel would.
class sharded_read_fixture
{
public:
	sharded_read_fixture()
		: m_destination(make_host_array<std::uint16_t>(
			{6, 10}, numerical_type::uint16, 9999))
	{
	}

	std::unique_ptr<zarr_read_operation> make_operation()
	{
		const auto directory = get_zarr_asset_path("v3_uint16_sharded.zarr");
		const auto layout = std::make_shared<const zarr_array_layout>(
			read_zarr_node(directory));
		const auto regions = whole_of({6, 10});
		image_chunk_regions chunks(
			regions,
			layout->get_shape(),
			layout->get_chunk_layout().get_extents()
		);

		return std::make_unique<zarr_read_operation>(
			directory, layout, m_destination.share(), std::move(chunks),
			true, 1UL << 20, 64);
	}

	std::unique_ptr<trompeloeil::expectation> take_tasks()
	{
		return NAMED_ALLOW_CALL(m_scope, run(_, _))
			.LR_SIDE_EFFECT( m_tasks[_2] = std::move(_1) );
	}

	std::unique_ptr<trompeloeil::expectation> take_reads()
	{
		return NAMED_ALLOW_CALL(m_scope, queue_read(_, _, _, _))
			.LR_SIDE_EFFECT(
				REQUIRE( _3.size() == 1 );
				m_reads.emplace(_4, queued{_1, _2, _3[0]})
			);
	}

	// Runs every task taken so far, of the step given, in order.
	void finish_all(zarr_read_operation &operation, std::uint64_t step)
	{
		std::vector<std::uint64_t> tags;
		for (const auto &task : m_tasks)
		{
			if ((task.first & 7) == step)
			{
				tags.push_back(task.first);
			}
		}
		for (const auto tag : tags)
		{
			auto work = std::move(m_tasks.at(tag));
			m_tasks.erase(tag);
			work->run();
			operation.on_task(tag, nullptr, m_scope);
		}
	}

	// Lands every read queued so far, of the step given, from its file.
	void land_all(zarr_read_operation &operation, std::uint64_t step)
	{
		std::vector<std::uint64_t> tags;
		for (const auto &read : m_reads)
		{
			if ((read.first & 7) == step)
			{
				tags.push_back(read.first);
			}
		}
		for (const auto tag : tags)
		{
			const auto read = m_reads.at(tag);
			m_reads.erase(tag);
			std::error_code error;
			const auto size = read_scattered(
				read.file, read.offset, make_span(&read.buffer, 1), error);
			REQUIRE_FALSE( error );
			operation.on_transfer(
				file_transfer_completion(tag, size), m_scope);
		}
	}

	struct queued
	{
		file_handle_t file;
		std::uint64_t offset;
		memory_range buffer;
	};

	array m_destination;
	mock_file_transfer_scope m_scope;
	std::map<std::uint64_t, std::unique_ptr<task>> m_tasks;
	std::map<std::uint64_t, queued> m_reads;
};

} // anonymous namespace

TEST_CASE( "zarr_read_operation reads the index of a shard, then its chunks",
	"[zarr_read_operation]" )
{
	sharded_read_fixture fixture;
	const auto tasks = fixture.take_tasks();
	const auto reads = fixture.take_reads();
	auto operation = fixture.make_operation();

	// Six shards, of which the last is not stored.
	operation->start(fixture.m_scope);
	REQUIRE( fixture.m_tasks.size() == 6 );
	fixture.finish_all(*operation, open_step);

	REQUIRE( fixture.m_reads.size() == 5 );
	for (const auto &read : fixture.m_reads)
	{
		REQUIRE( (read.first & 7) == read_index_step );
		REQUIRE( read.second.buffer.get_size() == 4 * 16 + 4 );
	}

	// Each index then asks for the chunks its shard stores: all four of the
	// shards inside the array, fewer at its edges, and three of the first,
	// which does not store the chunk at the origin.
	fixture.land_all(*operation, read_index_step);
	REQUIRE( fixture.m_reads.size() == 3 + 4 + 2 + 2 + 2 );

	fixture.land_all(*operation, read_chunk_step);
	fixture.finish_all(*operation, decode_step);
	REQUIRE( fixture.m_tasks.empty() );
	REQUIRE( fixture.m_reads.empty() );

	std::vector<std::uint16_t> expected(60);
	for (std::size_t row = 0; row < 6; ++row)
	{
		for (std::size_t column = 0; column < 10; ++column)
		{
			const bool filled =
				(row < 2 && column < 2) || (row >= 4 && column >= 8);
			expected[row * 10 + column] = filled
				? 0
				: static_cast<std::uint16_t>(3 * (row * 10 + column) + 5);
		}
	}
	REQUIRE( get_values<std::uint16_t>(fixture.m_destination) == expected );
}
