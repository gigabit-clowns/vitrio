// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/detector_event_position_view.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

using namespace vitrio;

namespace
{

using coordinates = std::vector<std::uint32_t>;

coordinates to_vector(span<const std::uint32_t> values)
{
	return coordinates(values.begin(), values.end());
}

} // anonymous namespace

TEST_CASE(
	"a detector_event_position_view views coordinates as rows of events",
	"[detector_event_position_view]" )
{
	const coordinates values = {0, 1,  2, 3,  4, 5};
	const detector_event_position_view positions(make_span(values), 2);

	SECTION( "its shape is a row per event and a column per axis" )
	{
		REQUIRE( positions.get_rank() == 2 );
		REQUIRE( positions.get_event_count() == 3 );
		REQUIRE_FALSE( positions.empty() );
	}

	SECTION( "a row is the position of an event" )
	{
		REQUIRE( to_vector(positions.get(0)) == coordinates{0, 1} );
		REQUIRE( to_vector(positions.get(2)) == coordinates{4, 5} );
	}

	SECTION( "an element is one coordinate of an event" )
	{
		REQUIRE( positions(1, 0) == 2 );
		REQUIRE( positions(1, 1) == 3 );
	}

	SECTION( "a run of rows is a view of the same rank" )
	{
		const auto run = positions.get_events(1, 2);

		REQUIRE( run.get_rank() == 2 );
		REQUIRE( run.get_event_count() == 2 );
		REQUIRE( run(0, 0) == 2 );
		REQUIRE( run(1, 1) == 5 );
	}

	SECTION( "an empty run views nothing" )
	{
		REQUIRE( positions.get_events(3, 0).empty() );
	}

	SECTION( "every coordinate is viewed row after row" )
	{
		REQUIRE( positions.get_coordinates().data() == values.data() );
		REQUIRE( to_vector(positions.get_coordinates()) == values );
	}
}

TEST_CASE(
	"a detector_event_position_view of no coordinates views no events",
	"[detector_event_position_view]" )
{
	const detector_event_position_view positions(
		span<const std::uint32_t>(), 3);

	REQUIRE( positions.get_rank() == 3 );
	REQUIRE( positions.get_event_count() == 0 );
	REQUIRE( positions.empty() );
}

TEST_CASE(
	"a detector_event_position_view refuses what is not a matrix",
	"[detector_event_position_view]" )
{
	const coordinates values = {1, 2, 3};

	SECTION( "a rank of zero" )
	{
		REQUIRE_THROWS_AS(
			detector_event_position_view(make_span(values), 0),
			std::invalid_argument
		);
	}

	SECTION( "coordinates that are not whole rows" )
	{
		REQUIRE_THROWS_AS(
			detector_event_position_view(make_span(values), 2),
			std::invalid_argument
		);
	}
}
