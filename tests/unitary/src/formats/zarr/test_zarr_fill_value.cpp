// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/zarr/zarr_fill_value.hpp>

#include <array/fixed_width_float.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <limits>

using namespace vitrio;
using namespace vitrio::zarr;
using nlohmann::json;

namespace
{

template <typename T>
T fill_as(const json &value, numerical_type type)
{
	const auto bytes = parse_fill_value(value, type, "test");
	REQUIRE( bytes.size() == sizeof(T) );
	T result;
	std::memcpy(&result, bytes.data(), sizeof(T));
	return result;
}

} // anonymous namespace

TEST_CASE( "parse_fill_value reads one element of the type",
	"[zarr_fill_value]" )
{
	SECTION( "integers that fit" )
	{
		REQUIRE( fill_as<std::int16_t>(-7, numerical_type::int16) == -7 );
		REQUIRE( fill_as<std::uint8_t>(255, numerical_type::uint8) == 255 );
		REQUIRE( fill_as<std::uint64_t>(
			json::parse("18446744073709551615"), numerical_type::uint64) ==
			std::numeric_limits<std::uint64_t>::max() );
		REQUIRE( fill_as<std::int64_t>(
			json::parse("-9223372036854775808"), numerical_type::int64) ==
			std::numeric_limits<std::int64_t>::min() );
		REQUIRE( fill_as<std::int32_t>(3.0, numerical_type::int32) == 3 );
	}

	SECTION( "integers that do not are malformed" )
	{
		REQUIRE_THROWS_AS(
			parse_fill_value(256, numerical_type::uint8, "test"),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			parse_fill_value(-1, numerical_type::uint16, "test"),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			parse_fill_value(0.5, numerical_type::int32, "test"),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			parse_fill_value(
				18446744073709551616.0, numerical_type::uint64, "test"),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			parse_fill_value("NaN", numerical_type::int32, "test"),
			image_file_format_error);
	}

	SECTION( "floating point numbers, and what is no number" )
	{
		REQUIRE( fill_as<float32_t>(0.25, numerical_type::float32) == 0.25F );
		REQUIRE( fill_as<float64_t>(-2, numerical_type::float64) == -2.0 );
		REQUIRE( std::isnan(fill_as<float32_t>(
			"NaN", numerical_type::float32)) );
		REQUIRE( fill_as<float64_t>("Infinity", numerical_type::float64) ==
			std::numeric_limits<float64_t>::infinity() );
		REQUIRE( fill_as<float32_t>("-Infinity", numerical_type::float32) ==
			-std::numeric_limits<float32_t>::infinity() );
		REQUIRE( static_cast<float>(fill_as<float16_t>(
			1.5, numerical_type::float16)) == 1.5F );
	}

	SECTION( "the bits of a floating point number in hexadecimal" )
	{
		REQUIRE( fill_as<std::uint32_t>(
			"0x3f800000", numerical_type::float32) == 0x3F800000U );
		REQUIRE( fill_as<std::uint16_t>(
			"0x7E00", numerical_type::float16) == 0x7E00U );
		REQUIRE( fill_as<std::uint64_t>(
			"0x3ff0000000000000", numerical_type::float64) ==
			0x3FF0000000000000ULL );
		REQUIRE_THROWS_AS(
			parse_fill_value("0x3f80", numerical_type::float32, "test"),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			parse_fill_value("0x3f80000g", numerical_type::float32, "test"),
			image_file_format_error);
	}

	SECTION( "a complex number as a pair" )
	{
		const auto value = fill_as<std::complex<float32_t>>(
			json::parse(R"([1.5, "NaN"])"), numerical_type::complex_float32);
		REQUIRE( value.real() == 1.5F );
		REQUIRE( std::isnan(value.imag()) );

		REQUIRE_THROWS_AS(
			parse_fill_value(1.5, numerical_type::complex_float64, "test"),
			image_file_format_error);
	}

	SECTION( "a boolean as one or as a number of version 2" )
	{
		REQUIRE( parse_fill_value(true, numerical_type::boolean, "test") ==
			std::vector<byte>{1} );
		REQUIRE( parse_fill_value(0, numerical_type::boolean, "test") ==
			std::vector<byte>{0} );
		REQUIRE_THROWS_AS(
			parse_fill_value(2, numerical_type::boolean, "test"),
			image_file_format_error);
	}

	SECTION( "no value is zero" )
	{
		REQUIRE( parse_fill_value(nullptr, numerical_type::float64, "test") ==
			std::vector<byte>(8, 0) );
	}

	SECTION( "the message starts with where it was read" )
	{
		REQUIRE_THROWS_WITH(
			parse_fill_value("x", numerical_type::float32, "path: here"),
			Catch::Matchers::StartsWith("path: here: ")
		);
	}
}
