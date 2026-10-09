// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/numerical_type.hpp>

#include <sstream>
#include <string>

using namespace vitrio;

TEST_CASE(
	"get_size reports the size of one element in bytes",
	"[numerical_type]"
)
{
	SECTION( "of a boolean" )
	{
		REQUIRE( get_size(numerical_type::boolean) == 1 );
	}

	SECTION( "of the integers" )
	{
		REQUIRE( get_size(numerical_type::int8) == 1 );
		REQUIRE( get_size(numerical_type::uint8) == 1 );
		REQUIRE( get_size(numerical_type::int16) == 2 );
		REQUIRE( get_size(numerical_type::uint16) == 2 );
		REQUIRE( get_size(numerical_type::int32) == 4 );
		REQUIRE( get_size(numerical_type::uint32) == 4 );
		REQUIRE( get_size(numerical_type::int64) == 8 );
		REQUIRE( get_size(numerical_type::uint64) == 8 );
	}

	SECTION( "of the floating point numbers" )
	{
		REQUIRE( get_size(numerical_type::float16) == 2 );
		REQUIRE( get_size(numerical_type::float32) == 4 );
		REQUIRE( get_size(numerical_type::float64) == 8 );
	}

	SECTION( "of the complex numbers, which are two of their component" )
	{
		REQUIRE( get_size(numerical_type::complex_float16) == 4 );
		REQUIRE( get_size(numerical_type::complex_float32) == 8 );
		REQUIRE( get_size(numerical_type::complex_float64) == 16 );
	}

	SECTION( "and zero for an unknown type" )
	{
		REQUIRE( get_size(numerical_type::unknown) == 0 );
	}
}

TEST_CASE(
	"to_string names a numerical type as its enumerator is spelled",
	"[numerical_type]"
)
{
	REQUIRE( to_string(numerical_type::boolean) == std::string("boolean") );
	REQUIRE( to_string(numerical_type::int8) == std::string("int8") );
	REQUIRE( to_string(numerical_type::uint8) == std::string("uint8") );
	REQUIRE( to_string(numerical_type::int16) == std::string("int16") );
	REQUIRE( to_string(numerical_type::uint16) == std::string("uint16") );
	REQUIRE( to_string(numerical_type::int32) == std::string("int32") );
	REQUIRE( to_string(numerical_type::uint32) == std::string("uint32") );
	REQUIRE( to_string(numerical_type::int64) == std::string("int64") );
	REQUIRE( to_string(numerical_type::uint64) == std::string("uint64") );
	REQUIRE( to_string(numerical_type::float16) == std::string("float16") );
	REQUIRE( to_string(numerical_type::float32) == std::string("float32") );
	REQUIRE( to_string(numerical_type::float64) == std::string("float64") );
	REQUIRE(
		to_string(numerical_type::complex_float16) ==
		std::string("complex_float16")
	);
	REQUIRE(
		to_string(numerical_type::complex_float32) ==
		std::string("complex_float32")
	);
	REQUIRE(
		to_string(numerical_type::complex_float64) ==
		std::string("complex_float64")
	);

	SECTION( "and gives an empty name to an unknown type" )
	{
		REQUIRE( to_string(numerical_type::unknown) == std::string() );
	}
}

TEST_CASE(
	"a numerical type is written to a stream as its name",
	"[numerical_type]"
)
{
	std::ostringstream text;

	text << numerical_type::float32 << ' ' << numerical_type::complex_float64;

	REQUIRE( text.str() == "float32 complex_float64" );
}
