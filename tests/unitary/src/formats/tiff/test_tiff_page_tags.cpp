// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/tiff/tiff_page_tags.hpp>

#include <string>

using namespace vitrio::tiff;

TEST_CASE( "the tags of a page state nothing until they are set",
	"[tiff_page_tags]" )
{
	const tiff_page_tags tags;

	REQUIRE( tags.get_x_resolution() == 0.0 );
	REQUIRE( tags.get_y_resolution() == 0.0 );
	REQUIRE( tags.get_image_description().empty() );
	REQUIRE( tags.get_software().empty() );
	REQUIRE( tags.get_date_time().empty() );
}

TEST_CASE( "the tags of a page hold what they are set to",
	"[tiff_page_tags]" )
{
	tiff_page_tags tags;
	tags.set_x_resolution(1.0e8);
	tags.set_y_resolution(2.0e8);
	tags.set_image_description("a micrograph");
	tags.set_software("something");
	tags.set_date_time("2026:11:26 12:00:00");

	SECTION( "each apart from the others" )
	{
		REQUIRE( tags.get_x_resolution() == 1.0e8 );
		REQUIRE( tags.get_y_resolution() == 2.0e8 );
		REQUIRE( tags.get_image_description() == "a micrograph" );
		REQUIRE( tags.get_software() == "something" );
		REQUIRE( tags.get_date_time() == "2026:11:26 12:00:00" );
	}

	SECTION( "and a copy holds the same" )
	{
		const tiff_page_tags copy(tags);

		REQUIRE( copy.get_x_resolution() == 1.0e8 );
		REQUIRE( copy.get_y_resolution() == 2.0e8 );
		REQUIRE( copy.get_image_description() == "a micrograph" );
		REQUIRE( copy.get_software() == "something" );
		REQUIRE( copy.get_date_time() == "2026:11:26 12:00:00" );
	}
}
