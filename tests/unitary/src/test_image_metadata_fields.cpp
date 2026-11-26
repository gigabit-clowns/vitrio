// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_metadata_fields.hpp>

#include <standard_image_metadata_field.hpp>

#include <string>

using namespace vitrio;

TEST_CASE(
	"The standard fields are labelled as they are named",
	"[image_metadata_fields]"
)
{
	using namespace image_metadata_fields;

	REQUIRE( std::string(pixel_size_x.get_label()) == "pixel_size_x" );
	REQUIRE( std::string(pixel_size_y.get_label()) == "pixel_size_y" );
	REQUIRE( std::string(pixel_size_z.get_label()) == "pixel_size_z" );
}

TEST_CASE(
	"The standard fields are set and got through their constants",
	"[image_metadata_fields]"
)
{
	using namespace image_metadata_fields;

	// A constant that the metadata did not know as a standard field would
	// be refused here, for its label has no dot.
	image_metadata metadata;
	set_field(metadata, pixel_size_x, 0.83);
	set_field(metadata, pixel_size_y, 0.84);
	set_field(metadata, pixel_size_z, 1.5);

	REQUIRE( get_field(metadata, pixel_size_x) == 0.83 );
	REQUIRE( get_field(metadata, pixel_size_y) == 0.84 );
	REQUIRE( get_field(metadata, pixel_size_z) == 1.5 );

	// As many as the metadata knows: a field added to one and not to the
	// other fails here.
	REQUIRE(
		metadata.get_entries().size() ==
		get_standard_image_metadata_fields().size()
	);
}
