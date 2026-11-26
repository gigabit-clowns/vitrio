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
#include <string>
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

namespace
{

// A datum at the start of a file of nothing else but its value.
dm_tag make_datum(
	const std::vector<std::uint64_t> &encoding,
	const std::vector<char> &raw
)
{
	return dm_tag("Value", encoding, 0, raw.size());
}

span<const byte> as_file(const std::vector<char> &raw)
{
	return make_span(reinterpret_cast<const byte*>(raw.data()), raw.size());
}

byte_order order_of(bool little_endian)
{
	return little_endian ? byte_order::little_endian : byte_order::big_endian;
}

} // anonymous namespace

TEST_CASE( "a real of either width is read in the byte order of the file",
	"[dm_tag_values]" )
{
	const bool little_endian = GENERATE(true, false);

	SECTION( "of 32 bits" )
	{
		const auto raw = payload_of(std::vector<float>{-0.375F}, little_endian);
		const auto tag = make_datum({encoding::float32}, raw);

		REQUIRE( holds_real(tag) );
		REQUIRE(
			read_real(tag, as_file(raw), order_of(little_endian)) == -0.375 );
	}

	SECTION( "of 64 bits" )
	{
		const auto raw = payload_of(std::vector<double>{0.1}, little_endian);
		const auto tag = make_datum({encoding::float64}, raw);

		REQUIRE( holds_real(tag) );
		REQUIRE( read_real(tag, as_file(raw), order_of(little_endian)) == 0.1 );
	}
}

TEST_CASE( "what is not a single real is not read as one", "[dm_tag_values]" )
{
	SECTION( "an integer" )
	{
		const auto raw = payload_of(std::vector<std::int32_t>{2}, true);
		const auto tag = make_datum({encoding::int32}, raw);

		REQUIRE_FALSE( holds_real(tag) );
		REQUIRE_THROWS_AS(
			read_real(tag, as_file(raw), byte_order::little_endian),
			image_file_format_error
		);
	}

	SECTION( "an array of reals" )
	{
		const auto raw = payload_of(std::vector<float>{1.0F, 2.0F}, true);
		const auto tag =
			make_datum({encoding::array, encoding::float32, 2}, raw);

		REQUIRE_FALSE( holds_real(tag) );
		REQUIRE_THROWS_AS(
			read_real(tag, as_file(raw), byte_order::little_endian),
			image_file_format_error
		);
	}

	SECTION( "a group" )
	{
		const dm_tag group("Group");

		REQUIRE_FALSE( holds_real(group) );
		REQUIRE_THROWS_AS(
			read_real(group, span<const byte>(), byte_order::little_endian),
			image_file_format_error
		);
	}
}

TEST_CASE( "a text is read in the byte order of the file", "[dm_tag_values]" )
{
	const bool little_endian = GENERATE(true, false);

	SECTION( "its characters, as the file holds them" )
	{
		// A micro sign and a letter: what a file calibrated in micrometres
		// states as its units.
		const std::vector<std::uint16_t> characters = {0x00B5, 'm'};
		const auto raw = payload_of(characters, little_endian);
		const auto tag =
			make_datum({encoding::array, encoding::uint16, 2}, raw);

		REQUIRE( holds_text(tag) );
		REQUIRE(
			read_text(tag, as_file(raw), order_of(little_endian)) ==
			u"\u00B5m"
		);
	}

	SECTION( "a text of no characters" )
	{
		const std::vector<char> raw;
		const auto tag =
			make_datum({encoding::array, encoding::uint16, 0}, raw);

		REQUIRE( holds_text(tag) );
		REQUIRE(
			read_text(tag, as_file(raw), order_of(little_endian)).empty() );
	}
}

TEST_CASE( "what is not a text is not read as one", "[dm_tag_values]" )
{
	SECTION( "an array of bytes" )
	{
		const auto raw = payload_of(std::vector<std::uint8_t>{'n', 'm'}, true);
		const auto tag = make_datum({encoding::array, encoding::uint8, 2}, raw);

		REQUIRE_FALSE( holds_text(tag) );
		REQUIRE_THROWS_AS(
			read_text(tag, as_file(raw), byte_order::little_endian),
			image_file_format_error
		);
	}

	SECTION( "a single character" )
	{
		const auto raw = payload_of(std::vector<std::uint16_t>{'n'}, true);
		const auto tag = make_datum({encoding::uint16}, raw);

		REQUIRE_FALSE( holds_text(tag) );
		REQUIRE_THROWS_AS(
			read_text(tag, as_file(raw), byte_order::little_endian),
			image_file_format_error
		);
	}

	SECTION( "a group" )
	{
		const dm_tag group("Group");

		REQUIRE_FALSE( holds_text(group) );
		REQUIRE_THROWS_AS(
			read_text(group, span<const byte>(), byte_order::little_endian),
			image_file_format_error
		);
	}
}
