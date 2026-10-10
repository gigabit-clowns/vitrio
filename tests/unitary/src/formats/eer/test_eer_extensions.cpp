// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/eer/eer_extensions.hpp>

using namespace vitrio::eer;

TEST_CASE( "the extension of an EER file is told", "[eer_extensions]" )
{
	REQUIRE( is_extension(".eer") );
	REQUIRE_FALSE( is_extension(".tif") );
	REQUIRE_FALSE( is_extension("eer") );
	REQUIRE_FALSE( is_extension("") );
}
