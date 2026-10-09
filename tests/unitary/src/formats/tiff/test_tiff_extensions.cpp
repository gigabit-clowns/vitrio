// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/tiff/tiff_extensions.hpp>

using namespace vitrio;
using namespace vitrio::tiff;

TEST_CASE( "the extensions a TIFF file is created with",
	"[tiff_extensions]" )
{
	SECTION( "the two names the format is known by are written" )
	{
		REQUIRE( is_writable_extension(".tif") );
		REQUIRE( is_writable_extension(".tiff") );
	}

	SECTION( "anything else is not" )
	{
		REQUIRE_FALSE( is_writable_extension("") );
		REQUIRE_FALSE( is_writable_extension("tif") );
		REQUIRE_FALSE( is_writable_extension(".TIF") );
		REQUIRE_FALSE( is_writable_extension(".mrc") );
		REQUIRE_FALSE( is_writable_extension(".eer") );
	}
}
