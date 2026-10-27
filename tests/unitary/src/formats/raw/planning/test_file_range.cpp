// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/raw/planning/file_range.hpp>

using namespace vitrio;

TEST_CASE( "a file_range is the stretch it was built from", "[file_range]" )
{
	const file_range range(1024, 48);

	CHECK( range.get_offset() == 1024 );
	CHECK( range.get_size() == 48 );
	CHECK( range.get_end() == 1072 );
}
