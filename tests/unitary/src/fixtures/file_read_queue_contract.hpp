// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <catch2/catch_test_macros.hpp>

#include "mapped_test_file.hpp"
#include "no_file.hpp"

#include <system/file_read_completion.hpp>
#include <system/file_read_queue.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vitrio
{

/**
 * @brief Take the completions of @p count outstanding reads, in tag order.
 */
inline std::vector<file_read_completion>
wait_for_reads(file_read_queue &queue, std::size_t count)
{
	std::vector<file_read_completion> completions;
	for (std::size_t index = 0; index < count; ++index)
	{
		completions.push_back(queue.wait());
	}

	std::sort(
		completions.begin(),
		completions.end(),
		[](const file_read_completion &a, const file_read_completion &b)
		{
			return a.get_tag() < b.get_tag();
		}
	);
	return completions;
}

/**
 * @brief Check that the queues a function makes keep the contract of a
 * @ref file_read_queue, in a section each.
 *
 * @param name The name of the file the queues read, which no other case of
 * the suite may use.
 * @param make_queue Makes a queue with no read outstanding, as a
 * std::unique_ptr.
 */
template <typename MakeQueue>
void check_file_read_queue_contract(
	const std::string &name,
	MakeQueue make_queue
)
{
	constexpr std::size_t file_size = 10000;
	const mapped_test_file file(name, file_size);
	const auto file_bytes = file.get_bytes();

	const auto bytes_at = [&](std::size_t offset, std::size_t size)
	{
		const auto first = file_bytes.begin() + offset;
		return std::vector<byte>(first, first + size);
	};

	// The buffers outlive the queue, which may still be reading into them
	// until it is destroyed.
	std::vector<std::vector<byte>> buffers;
	buffers.reserve(16);
	auto queue = make_queue();

	// Queues a read into a buffer of its own.
	const auto queue_read = [&](
		std::uint64_t offset,
		std::size_t size,
		std::uint64_t tag
	)
	{
		buffers.emplace_back(size);
		const memory_range buffer(buffers.back().data(), size);
		queue->queue(file.get_file(), offset, make_span(&buffer, 1), tag);
	};

	SECTION( "a read fills its buffers in order, from the first" )
	{
		std::vector<byte> first(7);
		std::vector<byte> second(13);
		const std::vector<memory_range> scattered = {
			{first.data(), first.size()},
			{second.data(), 0},
			{second.data(), second.size()},
		};

		queue->queue(file.get_file(), 100, make_span(scattered), 42);
		const auto completion = queue->wait();

		CHECK( completion.get_tag() == 42 );
		CHECK_FALSE( completion.get_error() );
		CHECK( completion.get_size() == 20 );
		CHECK( first == bytes_at(100, 7) );
		CHECK( second == bytes_at(107, 13) );
	}

	SECTION( "every outstanding read completes, carrying its tag" )
	{
		for (std::uint64_t tag = 0; tag < 8; ++tag)
		{
			queue_read(1000 * tag + 1, 300, tag);
		}
		const auto completions = wait_for_reads(*queue, 8);

		for (std::uint64_t tag = 0; tag < 8; ++tag)
		{
			CHECK( completions[tag].get_tag() == tag );
			CHECK( completions[tag].get_size() == 300 );
			CHECK( buffers[tag] == bytes_at(1000 * tag + 1, 300) );
		}
	}

	SECTION( "reads queued while others are outstanding complete too" )
	{
		queue_read(0, 100, 0);
		queue_read(100, 100, 1);
		auto completions = wait_for_reads(*queue, 1);
		for (std::uint64_t tag = 2; tag < 5; ++tag)
		{
			queue_read(100 * tag, 100, tag);
		}
		const auto rest = wait_for_reads(*queue, 4);
		completions.insert(completions.end(), rest.begin(), rest.end());

		std::vector<std::uint64_t> tags;
		for (const auto &completion : completions)
		{
			tags.push_back(completion.get_tag());
		}
		std::sort(tags.begin(), tags.end());
		CHECK( tags == std::vector<std::uint64_t>{0, 1, 2, 3, 4} );
		for (std::uint64_t tag = 0; tag < 5; ++tag)
		{
			CHECK( buffers[tag] == bytes_at(100 * tag, 100) );
		}
	}

	SECTION( "a read that reaches past the end of the file stops at it" )
	{
		std::vector<byte> first(30);
		std::vector<byte> second(30);
		const std::vector<memory_range> scattered = {
			{first.data(), first.size()},
			{second.data(), second.size()},
		};

		queue->queue(file.get_file(), file_size - 40, make_span(scattered), 0);
		const auto completion = queue->wait();

		CHECK_FALSE( completion.get_error() );
		CHECK( completion.get_size() == 40 );
		CHECK( first == bytes_at(file_size - 40, 30) );
		second.resize(10);
		CHECK( second == bytes_at(file_size - 10, 10) );
	}

	SECTION( "a read that starts at or past the end of the file reads nothing" )
	{
		queue_read(file_size, 20, 0);
		queue_read(file_size + 100, 20, 1);

		for (const auto &completion : wait_for_reads(*queue, 2))
		{
			CHECK_FALSE( completion.get_error() );
			CHECK( completion.get_size() == 0 );
		}
	}

	SECTION( "a read of no bytes completes with none" )
	{
		queue->queue(file.get_file(), 0, span<const memory_range>(), 0);
		queue_read(0, 0, 1);

		for (const auto &completion : wait_for_reads(*queue, 2))
		{
			CHECK_FALSE( completion.get_error() );
			CHECK( completion.get_size() == 0 );
		}
	}

	SECTION( "a read the system fails carries its error" )
	{
		std::vector<byte> read(20);
		const memory_range buffer(read.data(), read.size());

		queue->queue(no_file, 0, make_span(&buffer, 1), 7);
		const auto completion = queue->wait();

		CHECK( completion.get_tag() == 7 );
		CHECK( completion.get_error() );
		CHECK( completion.get_size() == 0 );
	}

	SECTION( "a read is scattered over more buffers than one call takes" )
	{
		// Buffers of one, two and three bytes in turn.
		std::vector<std::size_t> sizes;
		std::size_t total = 0;
		for (std::size_t index = 0; index < 3000; ++index)
		{
			sizes.push_back(index % 3 + 1);
			total += index % 3 + 1;
		}
		REQUIRE( total <= file_size );

		std::vector<byte> read(total);
		std::vector<memory_range> scattered;
		auto *next = read.data();
		for (const auto size : sizes)
		{
			scattered.emplace_back(next, size);
			next += size;
		}

		queue->queue(file.get_file(), 0, make_span(scattered), 0);
		const auto completion = queue->wait();

		CHECK( completion.get_size() == total );
		CHECK( read == bytes_at(0, total) );
	}

	SECTION( "destroying a queue waits for the reads that are outstanding" )
	{
		for (std::uint64_t tag = 0; tag < 5; ++tag)
		{
			queue_read(500 * tag, 400, tag);
		}
		queue.reset();

		for (std::uint64_t tag = 0; tag < 5; ++tag)
		{
			CHECK( buffers[tag] == bytes_at(500 * tag, 400) );
		}
	}
}

} // namespace vitrio
