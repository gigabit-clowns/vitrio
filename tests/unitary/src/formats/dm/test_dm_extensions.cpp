// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/dm/dm_extensions.hpp>

using namespace vitrio::dm;

TEST_CASE( "DigitalMicrograph files are read from their extensions",
	"[dm_extensions]" )
{
	REQUIRE( is_readable_extension(".dm3") );
	REQUIRE( is_readable_extension(".dm4") );

	REQUIRE_FALSE( is_readable_extension(".dm2") );
	REQUIRE_FALSE( is_readable_extension(".dm5") );
	REQUIRE_FALSE( is_readable_extension("dm4") );
	REQUIRE_FALSE( is_readable_extension(".mrc") );
	REQUIRE_FALSE( is_readable_extension("") );
}
