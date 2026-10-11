// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/dm/dm_data_type.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>

using namespace vitrio;
using namespace vitrio::dm;

TEST_CASE( "the data types of images are read from their codes",
	"[dm_data_type]" )
{
	REQUIRE( get_data_type(1) == numerical_type::int16 );
	REQUIRE( get_data_type(2) == numerical_type::float32 );
	REQUIRE( get_data_type(3) == numerical_type::complex_float32 );
	REQUIRE( get_data_type(6) == numerical_type::uint8 );
	REQUIRE( get_data_type(7) == numerical_type::int32 );
	REQUIRE( get_data_type(9) == numerical_type::int8 );
	REQUIRE( get_data_type(10) == numerical_type::uint16 );
	REQUIRE( get_data_type(11) == numerical_type::uint32 );
	REQUIRE( get_data_type(12) == numerical_type::float64 );
	REQUIRE( get_data_type(13) == numerical_type::complex_float64 );

	SECTION( "a binary image is read as bytes" )
	{
		REQUIRE( get_data_type(14) == numerical_type::uint8 );
	}
}

TEST_CASE( "the data types that are not read are refused", "[dm_data_type]" )
{
	SECTION( "packed complex values" )
	{
		REQUIRE_THROWS_AS( get_data_type(5), unsupported_operation_error );
	}

	SECTION( "RGB values" )
	{
		REQUIRE_THROWS_AS( get_data_type(8), unsupported_operation_error );
		REQUIRE_THROWS_AS( get_data_type(23), unsupported_operation_error );
	}

	SECTION( "codes the format is not known to write" )
	{
		REQUIRE_THROWS_AS( get_data_type(0), unsupported_operation_error );
		REQUIRE_THROWS_AS( get_data_type(4), unsupported_operation_error );
		REQUIRE_THROWS_AS( get_data_type(27), unsupported_operation_error );
		REQUIRE_THROWS_AS( get_data_type(-1), unsupported_operation_error );
	}
}
