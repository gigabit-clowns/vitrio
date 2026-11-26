// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <formats/tiff/tiff_metadata.hpp>

#include <vitrio/image_metadata_fields.hpp>

#include <limits>
#include <string>

using namespace vitrio;
using namespace vitrio::tiff;
using namespace vitrio::image_metadata_fields;
using Catch::Matchers::WithinRel;

namespace
{

// A pixel of an ångström is a hundred million pixels to a centimetre.
constexpr double per_centimetre = 1.0e8;

tiff_page_tags make_tags(double x_resolution, double y_resolution)
{
	tiff_page_tags tags;
	tags.set_x_resolution(x_resolution);
	tags.set_y_resolution(y_resolution);
	return tags;
}

image_metadata make_pixel_sizes(double x, double y)
{
	image_metadata metadata;
	set_field(metadata, pixel_size_x, x);
	set_field(metadata, pixel_size_y, y);
	return metadata;
}

} // anonymous namespace

TEST_CASE(
	"the pixel sizes of a TIFF page are a length over its resolution",
	"[tiff_metadata]"
)
{
	SECTION( "along both axes" )
	{
		const auto metadata = derive_metadata(
			make_tags(per_centimetre, 2.0 * per_centimetre));

		REQUIRE( get_field(metadata, pixel_size_x) == 1.0 );
		REQUIRE( get_field(metadata, pixel_size_y) == 0.5 );
		REQUIRE( find_field(metadata, pixel_size_z) == nullptr );
	}

	SECTION( "along the one axis the page states it for" )
	{
		const auto metadata =
			derive_metadata(make_tags(0.0, 4.0 * per_centimetre));

		REQUIRE( find_field(metadata, pixel_size_x) == nullptr );
		REQUIRE( get_field(metadata, pixel_size_y) == 0.25 );
	}

	SECTION( "and none of a page that states no resolution" )
	{
		REQUIRE( derive_metadata(tiff_page_tags()).get_entries().empty() );
	}

	SECTION( "nor of one that states what is no resolution" )
	{
		const auto metadata = derive_metadata(make_tags(
			-per_centimetre,
			std::numeric_limits<double>::quiet_NaN()
		));

		REQUIRE( metadata.get_entries().empty() );
		REQUIRE(
			derive_metadata(make_tags(
				std::numeric_limits<double>::infinity(), 0.0
			)).get_entries().empty()
		);
	}
}

TEST_CASE(
	"the texts of a TIFF page are stated under labels of the format",
	"[tiff_metadata]"
)
{
	tiff_page_tags tags;

	SECTION( "each of them" )
	{
		tags.set_image_description("a micrograph");
		tags.set_software("something");
		tags.set_date_time("2026:11:26 12:00:00");

		const auto metadata = derive_metadata(tags);

		REQUIRE(
			metadata.get("tiff.image_description").get_text() ==
			"a micrograph"
		);
		REQUIRE( metadata.get("tiff.software").get_text() == "something" );
		REQUIRE(
			metadata.get("tiff.date_time").get_text() ==
			"2026:11:26 12:00:00"
		);
	}

	SECTION( "and only those the page has" )
	{
		tags.set_software("something");

		const auto metadata = derive_metadata(tags);

		REQUIRE( metadata.get_entries().size() == 1 );
		REQUIRE( get_field(metadata, software_field) == "something" );
	}
}

TEST_CASE(
	"the pixel sizes given for a TIFF file become its resolution",
	"[tiff_metadata]"
)
{
	SECTION( "along both axes" )
	{
		const auto tags = make_page_tags(make_pixel_sizes(1.0, 0.5));

		REQUIRE( tags.get_x_resolution() == per_centimetre );
		REQUIRE( tags.get_y_resolution() == 2.0 * per_centimetre );
	}

	SECTION( "along the one axis one is given for" )
	{
		image_metadata metadata;
		set_field(metadata, pixel_size_y, 0.25);

		const auto tags = make_page_tags(metadata);

		REQUIRE( tags.get_x_resolution() == 0.0 );
		REQUIRE( tags.get_y_resolution() == 4.0 * per_centimetre );
	}

	SECTION( "an image has no z, so one given is left out" )
	{
		image_metadata metadata;
		set_field(metadata, pixel_size_z, 1.5);

		const auto tags = make_page_tags(metadata);

		REQUIRE( tags.get_x_resolution() == 0.0 );
		REQUIRE( tags.get_y_resolution() == 0.0 );
	}

	SECTION( "metadata that states nothing makes tags that state nothing" )
	{
		const auto tags = make_page_tags(image_metadata());

		REQUIRE( tags.get_x_resolution() == 0.0 );
		REQUIRE( tags.get_y_resolution() == 0.0 );
		REQUIRE( tags.get_image_description().empty() );
		REQUIRE( tags.get_software().empty() );
		REQUIRE( tags.get_date_time().empty() );
	}
}

TEST_CASE(
	"a pixel size a TIFF page can not hold is left out",
	"[tiff_metadata]"
)
{
	SECTION( "zero, or below it" )
	{
		const auto tags = make_page_tags(make_pixel_sizes(0.0, -1.0));

		REQUIRE( tags.get_x_resolution() == 0.0 );
		REQUIRE( tags.get_y_resolution() == 0.0 );
	}

	SECTION( "no number, or no finite one" )
	{
		const auto tags = make_page_tags(make_pixel_sizes(
			std::numeric_limits<double>::quiet_NaN(),
			std::numeric_limits<double>::infinity()
		));

		REQUIRE( tags.get_x_resolution() == 0.0 );
		REQUIRE( tags.get_y_resolution() == 0.0 );
	}

	// A resolution is a quotient of two integers of 32 bits.
	SECTION( "one whose resolution is above or below what a tag holds" )
	{
		const auto tags = make_page_tags(make_pixel_sizes(0.01, 1.0e18));

		REQUIRE( tags.get_x_resolution() == 0.0 );
		REQUIRE( tags.get_y_resolution() == 0.0 );
	}

	SECTION( "one whose resolution is just what a tag holds" )
	{
		const auto tags = make_page_tags(make_pixel_sizes(0.025, 1.0e17));

		REQUIRE_THAT( tags.get_x_resolution(), WithinRel(4.0e9, 1.0e-12) );
		REQUIRE_THAT( tags.get_y_resolution(), WithinRel(1.0e-9, 1.0e-12) );
	}
}

TEST_CASE(
	"the texts given for a TIFF file become its tags",
	"[tiff_metadata]"
)
{
	image_metadata metadata;

	SECTION( "each of them" )
	{
		metadata.set("tiff.image_description", "a micrograph");
		metadata.set("tiff.software", "something");
		metadata.set("tiff.date_time", "2026:11:26 12:00:00");

		const auto tags = make_page_tags(metadata);

		REQUIRE( tags.get_image_description() == "a micrograph" );
		REQUIRE( tags.get_software() == "something" );
		REQUIRE( tags.get_date_time() == "2026:11:26 12:00:00" );
	}

	SECTION( "a text with a null character, which would end it, is left "
		"out" )
	{
		metadata.set("tiff.software", std::string("some\0thing", 10));

		REQUIRE( make_page_tags(metadata).get_software().empty() );
	}

	SECTION( "what is stated as anything but a text is left out" )
	{
		metadata.set("tiff.image_description", 3);
		metadata.set("tiff.software", 1.5);
		metadata.set("tiff.date_time", 20261126);

		const auto tags = make_page_tags(metadata);

		REQUIRE( tags.get_image_description().empty() );
		REQUIRE( tags.get_software().empty() );
		REQUIRE( tags.get_date_time().empty() );
	}

	SECTION( "a date that is not as the format has it is left out" )
	{
		for (const std::string text : {
			"2026-11-26 12:00:00",
			"2026:11:26T12:00:00",
			"2026:11:26 12:00",
			"2026:11:26 12:00:00 ",
			"yesterday at noon",
			"2026:11:2x 12:00:00"
		})
		{
			image_metadata dated;
			dated.set("tiff.date_time", text);

			CHECK( make_page_tags(dated).get_date_time().empty() );
		}
	}
}

TEST_CASE(
	"what another format states is nothing to a TIFF file",
	"[tiff_metadata]"
)
{
	image_metadata metadata;
	metadata.set("mrc.labels", "Created by something else");
	metadata.set("application.frames", 40);

	const auto tags = make_page_tags(metadata);

	REQUIRE( tags.get_x_resolution() == 0.0 );
	REQUIRE( tags.get_image_description().empty() );
	REQUIRE( tags.get_software().empty() );
	REQUIRE( tags.get_date_time().empty() );
}

TEST_CASE(
	"what the tags of a TIFF page are made from is what they state",
	"[tiff_metadata]"
)
{
	auto metadata = make_pixel_sizes(0.5, 0.25);
	metadata.set("tiff.image_description", "a micrograph");
	metadata.set("tiff.software", "something");
	metadata.set("tiff.date_time", "2026:11:26 12:00:00");

	REQUIRE( derive_metadata(make_page_tags(metadata)) == metadata );
}
