// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <formats/dm/dm_constants.hpp>
#include <formats/dm/dm_tag.hpp>
#include <formats/dm/dm_tag_values.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/span.hpp>

#include "fixtures/dm_test_file.hpp"

#include <cstdint>
#include <vector>

using namespace vitrio;
using namespace vitrio::dm;
using namespace vitrio::test;

namespace
{

// Read a value laid out at the start of a file of nothing else.
template <typename T>
std::int64_t read_as(std::uint64_t type, T value, bool little_endian)
{
	const auto raw = payload_of(std::vector<T>{value}, little_endian);
	const dm_tag tag("Value", {type}, 0, raw.size());

	return read_integer(
		tag,
		make_span(reinterpret_cast<const byte*>(raw.data()), raw.size()),
		little_endian ? byte_order::little_endian : byte_order::big_endian
	);
}

} // anonymous namespace

TEST_CASE( "an integer of any width is read in the byte order of the file",
	"[dm_tag_values]" )
{
	const bool little_endian = GENERATE(true, false);

	REQUIRE( read_as<std::int8_t>(encoding::int8, -7, little_endian) == -7 );
	REQUIRE( read_as<std::uint8_t>(encoding::uint8, 250, little_endian) ==
		250 );
	REQUIRE( read_as<std::int16_t>(encoding::int16, -300, little_endian) ==
		-300 );
	REQUIRE( read_as<std::uint16_t>(encoding::uint16, 65000, little_endian)
		== 65000 );
	REQUIRE( read_as<std::int32_t>(encoding::int32, -70000, little_endian)
		== -70000 );
	REQUIRE( read_as<std::uint32_t>(
		encoding::uint32, 4000000000u, little_endian) == 4000000000 );
	REQUIRE( read_as<std::int64_t>(
		encoding::int64, -(1LL << 40), little_endian) == -(1LL << 40) );
	REQUIRE( read_as<std::uint64_t>(
		encoding::uint64, 1ULL << 62, little_endian) == (1LL << 62) );
}

TEST_CASE( "what is not a single integer is refused", "[dm_tag_values]" )
{
	SECTION( "an unsigned integer too large to read" )
	{
		REQUIRE_THROWS_AS(
			read_as<std::uint64_t>(encoding::uint64, ~0ULL, true),
			image_file_format_error
		);
	}

	SECTION( "a floating point value" )
	{
		REQUIRE_THROWS_AS(
			read_as<float>(encoding::float32, 1.0f, true),
			image_file_format_error
		);
	}

	SECTION( "a boolean" )
	{
		REQUIRE_THROWS_AS(
			read_as<std::uint8_t>(encoding::boolean, 1, true),
			image_file_format_error
		);
	}

	SECTION( "an array of integers" )
	{
		const std::vector<char> raw(8, '\0');
		const dm_tag tag(
			"Values",
			{encoding::array, encoding::int32, 2},
			0,
			raw.size()
		);

		REQUIRE_THROWS_AS(
			read_integer(
				tag,
				make_span(
					reinterpret_cast<const byte*>(raw.data()),
					raw.size()
				),
				byte_order::little_endian
			),
			image_file_format_error
		);
	}

	SECTION( "a group" )
	{
		REQUIRE_THROWS_AS(
			read_integer(
				dm_tag("Group"),
				span<const byte>(),
				byte_order::little_endian
			),
			image_file_format_error
		);
	}
}
