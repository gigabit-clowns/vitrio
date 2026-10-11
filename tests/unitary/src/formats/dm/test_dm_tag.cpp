// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/dm/dm_constants.hpp>
#include <formats/dm/dm_tag.hpp>

#include <cstdint>
#include <utility>
#include <vector>

using namespace vitrio;
using namespace vitrio::dm;

TEST_CASE( "a group holds its entries in order", "[dm_tag]" )
{
	dm_tag group("ImageData");
	group.add_child(dm_tag("Calibrations"));
	group.add_child(dm_tag("DataType", {encoding::int32}, 100, 4));
	group.add_child(dm_tag("", {encoding::uint32}, 104, 4));
	group.add_child(dm_tag("DataType", {encoding::int16}, 108, 2));

	REQUIRE( group.is_group() );
	REQUIRE( group.get_label() == "ImageData" );
	REQUIRE( group.get_child_count() == 4 );
	REQUIRE( group.get_child(0).is_group() );
	REQUIRE( group.get_child(2).get_label().empty() );
	REQUIRE( group.get_encoding().size() == 0 );
	REQUIRE( group.get_payload_offset() == 0 );
	REQUIRE( group.get_payload_size() == 0 );

	SECTION( "the first entry of a label is found" )
	{
		const auto *found = group.find_child("DataType");

		REQUIRE( found == &group.get_child(1) );
	}

	SECTION( "a label it does not hold is not" )
	{
		REQUIRE( group.find_child("Data") == nullptr );
	}

	SECTION( "it keeps its entries when moved" )
	{
		const auto *first = &group.get_child(0);
		const dm_tag moved(std::move(group));

		REQUIRE( moved.get_child_count() == 4 );
		REQUIRE( &moved.get_child(0) == first );
	}
}

TEST_CASE( "a datum states where its value lies and how it is encoded",
	"[dm_tag]" )
{
	const std::vector<std::uint64_t> words = {
		encoding::array, encoding::float32, 12
	};
	const dm_tag datum("Data", words, 2048, 48);

	REQUIRE_FALSE( datum.is_group() );
	REQUIRE( datum.get_label() == "Data" );
	REQUIRE( std::vector<std::uint64_t>(
		datum.get_encoding().begin(),
		datum.get_encoding().end()) == words );
	REQUIRE( datum.get_payload_offset() == 2048 );
	REQUIRE( datum.get_payload_size() == 48 );
	REQUIRE( datum.get_child_count() == 0 );
	REQUIRE( datum.find_child("") == nullptr );
}
