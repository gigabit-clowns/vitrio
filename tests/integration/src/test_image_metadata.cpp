// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_metadata.hpp>
#include <vitrio/image_metadata_fields.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

using namespace vitrio;

namespace
{

// A field of an application, declared as a consumer declares one.
constexpr image_metadata_field<std::int64_t> frames{"application.frames"};

} // anonymous namespace

TEST_CASE(
	"A consumer states and reads metadata through the shared library",
	"[image_metadata]"
)
{
	using namespace image_metadata_fields;

	image_metadata metadata;
	set_field(metadata, pixel_size_x, 0.83);
	set_field(metadata, pixel_size_y, 1);
	set_field(metadata, frames, 40);
	metadata.set("application.session", "krios-3");

	SECTION( "through the fields" )
	{
		REQUIRE( get_field(metadata, pixel_size_x) == 0.83 );
		REQUIRE( get_field(metadata, pixel_size_y) == 1.0 );
		REQUIRE( get_field(metadata, frames) == 40 );
		REQUIRE( find_field(metadata, pixel_size_z) == nullptr );
		REQUIRE_THROWS_AS(
			get_field(metadata, pixel_size_z),
			std::out_of_range
		);
	}

	SECTION( "through the labels" )
	{
		REQUIRE( metadata.get("pixel_size_x").get_real() == 0.83 );
		REQUIRE( metadata.get("application.frames").get_integer() == 40 );
		REQUIRE(
			metadata.get("application.session").get_text() == "krios-3"
		);
		REQUIRE( metadata.get_entries().size() == 4 );
	}

	SECTION( "and is refused what the metadata does not hold" )
	{
		REQUIRE_THROWS_AS(
			metadata.set("pixel_size_x", "0.83"),
			std::invalid_argument
		);
		REQUIRE_THROWS_AS(
			metadata.set("session", "krios-3"),
			std::invalid_argument
		);
		REQUIRE_THROWS_AS(
			image_metadata_value(~std::uint64_t(0)),
			std::out_of_range
		);
	}
}
