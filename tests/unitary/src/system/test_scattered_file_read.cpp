// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/scattered_file_read.hpp>

#include "../fixtures/mapped_test_file.hpp"
#include "../fixtures/no_file.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/memory_range.hpp>

#include <cstddef>
#include <system_error>
#include <vector>

using namespace vitrio;

TEST_CASE(
	"read_scattered reads a stretch of a file into buffers in order",
	"[scattered_file_read]"
)
{
	constexpr std::size_t file_size = 5000;
	const mapped_test_file file("scattered_file_read.raw", file_size);
	const auto file_bytes = file.get_bytes();
	const auto bytes_at = [&](std::size_t offset, std::size_t size)
	{
		const auto first = file_bytes.begin() + offset;
		return std::vector<byte>(first, first + size);
	};

	std::error_code error = std::make_error_code(std::errc::io_error);

	SECTION( "buffers of every size, more of them than one call takes" )
	{
		// 300 buffers of one to seven bytes, some of none.
		std::vector<std::size_t> sizes;
		std::size_t total = 0;
		for (std::size_t index = 0; index < 300; ++index)
		{
			sizes.push_back(index % 8);
			total += index % 8;
		}

		std::vector<byte> read(total);
		std::vector<memory_range> buffers;
		auto *next = read.data();
		for (const auto size : sizes)
		{
			buffers.emplace_back(next, size);
			next += size;
		}

		CHECK( read_scattered(file.get_file(), 10, make_span(buffers), error)
			== total );
		CHECK_FALSE( error );
		CHECK( read == bytes_at(10, total) );
	}

	SECTION( "a stretch that reaches past the end of the file" )
	{
		std::vector<byte> first(30);
		std::vector<byte> second(30);
		const std::vector<memory_range> buffers = {
			{first.data(), first.size()},
			{second.data(), second.size()},
		};

		CHECK( read_scattered(
			file.get_file(),
			file_size - 40,
			make_span(buffers),
			error
		) == 40 );
		CHECK_FALSE( error );
		CHECK( first == bytes_at(file_size - 40, 30) );
		second.resize(10);
		CHECK( second == bytes_at(file_size - 10, 10) );
	}

	SECTION( "a stretch past the end of the file" )
	{
		std::vector<byte> read(30);
		const memory_range buffer(read.data(), read.size());

		CHECK( read_scattered(
			file.get_file(),
			file_size + 1,
			make_span(&buffer, 1),
			error
		) == 0 );
		CHECK_FALSE( error );
	}

	SECTION( "no buffer" )
	{
		CHECK( read_scattered(
			file.get_file(),
			0,
			span<const memory_range>(),
			error
		) == 0 );
		CHECK_FALSE( error );
	}

	SECTION( "a file that is not open" )
	{
		std::vector<byte> read(30);
		const memory_range buffer(read.data(), read.size());

		CHECK( read_scattered(no_file, 0, make_span(&buffer, 1), error) == 0 );
		CHECK( error );
	}
}
