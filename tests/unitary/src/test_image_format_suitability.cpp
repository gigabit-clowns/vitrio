// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_format_suitability.hpp>

#include <sstream>
#include <string>

using namespace vitrio;

TEST_CASE(
	"image_format_suitability orders its values from unsupported to optimal",
	"[image_format_suitability]"
)
{
	REQUIRE(
		image_format_suitability::unsupported <
		image_format_suitability::fallback
	);
	REQUIRE(
		image_format_suitability::fallback < image_format_suitability::normal
	);
	REQUIRE(
		image_format_suitability::normal < image_format_suitability::optimal
	);

	SECTION( "and the other comparisons agree with it" )
	{
		const auto low = image_format_suitability::fallback;
		const auto high = image_format_suitability::optimal;

		REQUIRE( high > low );
		REQUIRE( low <= high );
		REQUIRE( high >= low );

		REQUIRE_FALSE( low > high );
		REQUIRE_FALSE( high <= low );
		REQUIRE_FALSE( low >= high );
	}

	SECTION( "a value is neither below nor above itself" )
	{
		const auto value = image_format_suitability::normal;

		REQUIRE_FALSE( value < value );
		REQUIRE_FALSE( value > value );
		REQUIRE( value <= value );
		REQUIRE( value >= value );
	}
}

TEST_CASE(
	"image_format_suitability is compared in a constant expression",
	"[image_format_suitability]"
)
{
	static_assert(
		image_format_suitability::normal > image_format_suitability::fallback,
		"The comparison is constant"
	);
	static_assert(
		to_string(image_format_suitability::normal)[0] == 'n',
		"The name is constant"
	);

	SUCCEED();
}

TEST_CASE(
	"to_string names an image_format_suitability as its enumerator is spelled",
	"[image_format_suitability]"
)
{
	REQUIRE(
		to_string(image_format_suitability::unsupported) ==
		std::string("unsupported")
	);
	REQUIRE(
		to_string(image_format_suitability::fallback) ==
		std::string("fallback")
	);
	REQUIRE(
		to_string(image_format_suitability::normal) == std::string("normal")
	);
	REQUIRE(
		to_string(image_format_suitability::optimal) == std::string("optimal")
	);

	SECTION( "and gives an empty name to a value that is none of them" )
	{
		const auto other = static_cast<image_format_suitability>(7);

		REQUIRE( to_string(other) == std::string() );
	}
}

TEST_CASE(
	"an image_format_suitability is written to a stream as its name",
	"[image_format_suitability]"
)
{
	std::ostringstream text;

	text << image_format_suitability::optimal;

	REQUIRE( text.str() == "optimal" );
}
