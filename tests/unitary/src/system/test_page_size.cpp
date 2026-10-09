// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/page_size.hpp>

using namespace vitrio;

TEST_CASE(
	"get_page_size reports a size memory can be divided in",
	"[page_size]"
)
{
	const auto page = get_page_size();

	SECTION( "it is not zero" )
	{
		REQUIRE( page > 0 );
	}

	SECTION( "it is a power of two" )
	{
		REQUIRE( (page & (page - 1)) == 0 );
	}

	SECTION( "it does not change" )
	{
		REQUIRE( get_page_size() == page );
	}
}
