// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/dm/dm_constants.hpp>
#include <formats/dm/dm_encoding.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/span.hpp>

#include <cstdint>
#include <vector>

using namespace vitrio;
using namespace vitrio::dm;

namespace
{

std::size_t size_of(const std::vector<std::uint64_t> &words)
{
	return get_encoded_size(make_span(words));
}

} // anonymous namespace

TEST_CASE( "the scalars of the format have their sizes", "[dm_encoding]" )
{
	REQUIRE( get_scalar_size(encoding::boolean) == 1 );
	REQUIRE( get_scalar_size(encoding::int8) == 1 );
	REQUIRE( get_scalar_size(encoding::uint8) == 1 );
	REQUIRE( get_scalar_size(encoding::int16) == 2 );
	REQUIRE( get_scalar_size(encoding::uint16) == 2 );
	REQUIRE( get_scalar_size(encoding::int32) == 4 );
	REQUIRE( get_scalar_size(encoding::uint32) == 4 );
	REQUIRE( get_scalar_size(encoding::float32) == 4 );
	REQUIRE( get_scalar_size(encoding::int64) == 8 );
	REQUIRE( get_scalar_size(encoding::uint64) == 8 );
	REQUIRE( get_scalar_size(encoding::float64) == 8 );

	SECTION( "what is not a scalar has none" )
	{
		REQUIRE( get_scalar_size(encoding::structure) == 0 );
		REQUIRE( get_scalar_size(encoding::string) == 0 );
		REQUIRE( get_scalar_size(encoding::array) == 0 );
		REQUIRE( get_scalar_size(0) == 0 );
		REQUIRE( get_scalar_size(13) == 0 );
	}
}

TEST_CASE( "the integers of the format are told apart", "[dm_encoding]" )
{
	REQUIRE( is_integer(encoding::int8) );
	REQUIRE( is_integer(encoding::uint8) );
	REQUIRE( is_integer(encoding::int16) );
	REQUIRE( is_integer(encoding::uint16) );
	REQUIRE( is_integer(encoding::int32) );
	REQUIRE( is_integer(encoding::uint32) );
	REQUIRE( is_integer(encoding::int64) );
	REQUIRE( is_integer(encoding::uint64) );

	REQUIRE_FALSE( is_integer(encoding::boolean) );
	REQUIRE_FALSE( is_integer(encoding::float32) );
	REQUIRE_FALSE( is_integer(encoding::float64) );
	REQUIRE_FALSE( is_integer(encoding::array) );
}

TEST_CASE( "the size of a value is worked out from its encoding",
	"[dm_encoding]" )
{
	SECTION( "a scalar" )
	{
		REQUIRE( size_of({encoding::float64}) == 8 );
	}

	SECTION( "a string, a byte to a character" )
	{
		REQUIRE( size_of({encoding::string, 5}) == 5 );
	}

	SECTION( "a structure, the sum of its fields" )
	{
		REQUIRE( size_of({
			encoding::structure, 0, 3,
			0, encoding::int16,
			0, encoding::float32,
			0, encoding::uint8
		}) == 7 );
	}

	SECTION( "an array of scalars" )
	{
		REQUIRE( size_of({encoding::array, encoding::uint16, 10}) == 20 );
	}

	SECTION( "an array of structures" )
	{
		REQUIRE( size_of({
			encoding::array,
			encoding::structure, 0, 2,
			0, encoding::float32,
			0, encoding::float32,
			6
		}) == 48 );
	}

	SECTION( "an array of arrays" )
	{
		REQUIRE( size_of({
			encoding::array,
			encoding::array, encoding::int32, 3,
			4
		}) == 48 );
	}

	SECTION( "an empty array" )
	{
		REQUIRE( size_of({encoding::array, encoding::float32, 0}) == 0 );
	}
}

TEST_CASE( "an encoding that is not one is refused", "[dm_encoding]" )
{
	SECTION( "no words" )
	{
		REQUIRE_THROWS_AS( size_of({}), image_file_format_error );
	}

	SECTION( "a type the format does not have" )
	{
		REQUIRE_THROWS_AS( size_of({13}), image_file_format_error );
	}

	SECTION( "a structure with a field that is not a scalar" )
	{
		REQUIRE_THROWS_AS(
			size_of({encoding::structure, 0, 1, 0, encoding::string}),
			image_file_format_error
		);
	}

	SECTION( "words that end before the type does" )
	{
		REQUIRE_THROWS_AS(
			size_of({encoding::array, encoding::float32}),
			image_file_format_error
		);
		REQUIRE_THROWS_AS(
			size_of({encoding::structure, 0, 2, 0, encoding::int8}),
			image_file_format_error
		);
	}

	SECTION( "words past the type" )
	{
		REQUIRE_THROWS_AS(
			size_of({encoding::int32, encoding::int32}),
			image_file_format_error
		);
	}

	SECTION( "arrays nested too deep" )
	{
		// As many arrays as types may be nested, and a scalar inside them
		// one deeper.
		std::vector<std::uint64_t> words;
		for (std::size_t depth = 0; depth < max_encoding_depth; ++depth)
		{
			words.push_back(encoding::array);
		}
		words.push_back(encoding::uint8);
		for (std::size_t depth = 0; depth < max_encoding_depth; ++depth)
		{
			words.push_back(1);
		}

		REQUIRE_THROWS_AS( size_of(words), image_file_format_error );
		words.erase(words.begin());
		words.pop_back();
		REQUIRE( size_of(words) == 1 );
	}

	SECTION( "a size too large to address" )
	{
		REQUIRE_THROWS_AS(
			size_of({encoding::array, encoding::float64, 1ULL << 61}),
			image_file_format_error
		);
		REQUIRE_THROWS_AS(
			size_of({encoding::string, ~0ULL}),
			image_file_format_error
		);
	}
}
