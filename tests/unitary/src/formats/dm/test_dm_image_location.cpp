// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/dm/dm_image_location.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_descriptor.hpp>

#include <cstddef>
#include <vector>

using namespace vitrio;
using namespace vitrio::dm;

TEST_CASE( "the location of an image holds what it was given",
	"[dm_image_location]" )
{
	const std::vector<std::size_t> extents = {3, 4, 5};
	const image_descriptor descriptor(
		make_span(extents),
		2,
		numerical_type::uint16
	);

	const dm_image_location location(
		descriptor,
		1977,
		byte_order::big_endian
	);

	REQUIRE( location.get_descriptor() == descriptor );
	REQUIRE( location.get_data_offset() == 1977 );
	REQUIRE( location.get_byte_order() == byte_order::big_endian );
}
