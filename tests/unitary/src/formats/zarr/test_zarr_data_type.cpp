// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_data_type.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>

using namespace vitrio;
using namespace vitrio::zarr;
using nlohmann::json;

TEST_CASE( "parse_v2_data_type reads typestrings", "[zarr_data_type]" )
{
	SECTION( "each type an image holds, in either order" )
	{
		const auto little = parse_v2_data_type("<f4", "test");
		REQUIRE( little.type == numerical_type::float32 );
		REQUIRE( little.order == byte_order::little_endian );

		const auto big = parse_v2_data_type(">i2", "test");
		REQUIRE( big.type == numerical_type::int16 );
		REQUIRE( big.order == byte_order::big_endian );

		REQUIRE( parse_v2_data_type("<u8", "test").type ==
			numerical_type::uint64 );
		REQUIRE( parse_v2_data_type("<f2", "test").type ==
			numerical_type::float16 );
		REQUIRE( parse_v2_data_type("<c8", "test").type ==
			numerical_type::complex_float32 );
		REQUIRE( parse_v2_data_type(">c16", "test").type ==
			numerical_type::complex_float64 );
	}

	SECTION( "a type of one byte may state no order" )
	{
		REQUIRE( parse_v2_data_type("|u1", "test").type ==
			numerical_type::uint8 );
		REQUIRE( parse_v2_data_type("|i1", "test").type ==
			numerical_type::int8 );
		REQUIRE( parse_v2_data_type("|b1", "test").type ==
			numerical_type::boolean );
	}

	SECTION( "a wider one must" )
	{
		REQUIRE_THROWS_AS(
			parse_v2_data_type("|f4", "test"), image_file_format_error);
	}

	SECTION( "what no image holds is refused as unsupported" )
	{
		REQUIRE_THROWS_AS(
			parse_v2_data_type("<U12", "test"), unsupported_operation_error);
		REQUIRE_THROWS_AS(
			parse_v2_data_type("<M8[ns]", "test"),
			unsupported_operation_error);
		REQUIRE_THROWS_AS(
			parse_v2_data_type(json::parse(R"([["r", "|u1"]])"), "test"),
			unsupported_operation_error);
	}

	SECTION( "what is no typestring is malformed" )
	{
		REQUIRE_THROWS_AS(
			parse_v2_data_type(4, "test"), image_file_format_error);
		REQUIRE_THROWS_AS(
			parse_v2_data_type("f4", "test"), image_file_format_error);
	}
}

TEST_CASE( "parse_v3_data_type reads names", "[zarr_data_type]" )
{
	SECTION( "each name takes the order it is given" )
	{
		const auto type = parse_v3_data_type(
			"uint16", byte_order::big_endian, "test");
		REQUIRE( type.type == numerical_type::uint16 );
		REQUIRE( type.order == byte_order::big_endian );

		REQUIRE( parse_v3_data_type(
			"bool", byte_order::little_endian, "test").type ==
			numerical_type::boolean );
		REQUIRE( parse_v3_data_type(
			"complex128", byte_order::little_endian, "test").type ==
			numerical_type::complex_float64 );
	}

	SECTION( "an object that names the type" )
	{
		REQUIRE( parse_v3_data_type(
			json::parse(R"({"name": "float32"})"),
			byte_order::little_endian, "test").type ==
			numerical_type::float32 );
	}

	SECTION( "other types are refused as unsupported" )
	{
		REQUIRE_THROWS_AS(
			parse_v3_data_type("r16", byte_order::little_endian, "test"),
			unsupported_operation_error);
		REQUIRE_THROWS_AS(
			parse_v3_data_type("string", byte_order::little_endian, "test"),
			unsupported_operation_error);
	}

	SECTION( "what names no type is malformed" )
	{
		REQUIRE_THROWS_AS(
			parse_v3_data_type(3, byte_order::little_endian, "test"),
			image_file_format_error);
	}
}
