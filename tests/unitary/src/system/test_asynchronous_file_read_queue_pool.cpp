// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/asynchronous_file_read_queue_pool.hpp>

#include "../fixtures/mapped_test_file.hpp"

#include <system/file_read_completion.hpp>
#include <system/file_read_queue.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/memory_range.hpp>

#include <algorithm>
#include <cstddef>
#include <thread>
#include <vector>

using namespace vitrio;

namespace
{

// Whether a read of a stretch of the file through the queue gets its bytes.
bool reads_through(file_read_queue &queue, const mapped_test_file &file)
{
	constexpr std::size_t offset = 200;
	std::vector<byte> read(100);
	const memory_range buffer(read.data(), read.size());
	queue.queue(file.get_file(), offset, make_span(&buffer, 1), 0);

	const auto completion = queue.wait();
	const auto bytes = file.get_bytes();
	return
		!completion.get_error() &&
		completion.get_size() == read.size() &&
		std::equal(read.begin(), read.end(), bytes.begin() + offset);
}

} // anonymous namespace

TEST_CASE(
	"asynchronous_file_read_queue_pool gives each thread a queue of its own",
	"[asynchronous_file_read_queue_pool]"
)
{
	if (!asynchronous_file_read_queue_pool::is_supported())
	{
		SKIP( "The process can not have asynchronous queues here." );
	}

	const mapped_test_file file("asynchronous_file_read_queue_pool.raw", 1000);
	asynchronous_file_read_queue_pool pool;
	auto &queue = pool.get_queue();

	SECTION( "the same one on every call" )
	{
		CHECK( &pool.get_queue() == &queue );
	}

	SECTION( "the same one from every pool" )
	{
		asynchronous_file_read_queue_pool other;

		CHECK( &other.get_queue() == &queue );
	}

	SECTION( "another one on another thread" )
	{
		bool shared = true;
		bool read = false;
		std::thread other([&]
		{
			auto &own = pool.get_queue();
			shared = &own == &queue;
			read = reads_through(own, file);
		});
		other.join();

		CHECK_FALSE( shared );
		CHECK( read );
	}

	SECTION( "one that reads" )
	{
		CHECK( reads_through(queue, file) );
	}
}

TEST_CASE(
	"asynchronous_file_read_queue_pool::is_supported does not change",
	"[asynchronous_file_read_queue_pool]"
)
{
	const auto supported = asynchronous_file_read_queue_pool::is_supported();

	CHECK( asynchronous_file_read_queue_pool::is_supported() == supported );
}
