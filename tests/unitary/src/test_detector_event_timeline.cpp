// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/detector_event_timeline.hpp>

#include <vitrio/detector_event_position_view.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace vitrio;

namespace
{

using coordinates = std::vector<std::uint32_t>;

coordinates to_vector(detector_event_position_view positions)
{
	const auto values = positions.get_coordinates();
	return coordinates(values.begin(), values.end());
}

void add(
	detector_event_timeline &timeline,
	std::uint64_t timestamp,
	const coordinates &events
)
{
	timeline.add_group(
		timestamp,
		detector_event_position_view(make_span(events), timeline.get_rank())
	);
}

} // anonymous namespace

TEST_CASE(
	"a detector_event_timeline starts empty",
	"[detector_event_timeline]" )
{
	const detector_event_timeline timeline(2);

	REQUIRE( timeline.get_rank() == 2 );
	REQUIRE( timeline.get_group_count() == 0 );
	REQUIRE( timeline.get_event_count() == 0 );
	REQUIRE( timeline.get_positions().empty() );
}

TEST_CASE(
	"a detector_event_timeline refuses a rank of zero",
	"[detector_event_timeline]" )
{
	REQUIRE_THROWS_AS( detector_event_timeline(0), std::invalid_argument );
}

TEST_CASE(
	"a detector_event_timeline reads back the groups it was given",
	"[detector_event_timeline]" )
{
	detector_event_timeline timeline(2);
	add(timeline, 3, {0, 1, 2, 3});
	add(timeline, 4, {});
	add(timeline, 9, {5, 6});

	REQUIRE( timeline.get_group_count() == 3 );
	REQUIRE( timeline.get_event_count() == 3 );

	REQUIRE( timeline.get_timestamp(0) == 3 );
	REQUIRE( timeline.get_timestamp(1) == 4 );
	REQUIRE( timeline.get_timestamp(2) == 9 );

	REQUIRE( to_vector(timeline.get_positions(0)) ==
		coordinates{0, 1, 2, 3} );
	REQUIRE( to_vector(timeline.get_positions(1)).empty() );
	REQUIRE( to_vector(timeline.get_positions(2)) == coordinates{5, 6} );

	REQUIRE( to_vector(timeline.get_positions()) ==
		coordinates{0, 1, 2, 3, 5, 6} );
}

TEST_CASE(
	"a detector_event_timeline refuses groups out of order",
	"[detector_event_timeline]" )
{
	detector_event_timeline timeline(1);
	add(timeline, 5, {1});

	SECTION( "an earlier timestamp" )
	{
		REQUIRE_THROWS_AS( add(timeline, 4, {1}), std::invalid_argument );
	}

	SECTION( "the same timestamp" )
	{
		REQUIRE_THROWS_AS( add(timeline, 5, {1}), std::invalid_argument );
	}

	REQUIRE( timeline.get_group_count() == 1 );
	REQUIRE( timeline.get_event_count() == 1 );
}

TEST_CASE(
	"a detector_event_timeline refuses positions of another rank",
	"[detector_event_timeline]" )
{
	detector_event_timeline timeline(2);
	const coordinates events = {1, 2, 3};

	REQUIRE_THROWS_AS(
		timeline.add_group(
			0, detector_event_position_view(make_span(events), 3)),
		std::invalid_argument
	);
	REQUIRE( timeline.get_group_count() == 0 );
	REQUIRE( timeline.get_event_count() == 0 );
}

TEST_CASE(
	"a detector_event_timeline views each group as rows of positions",
	"[detector_event_timeline]" )
{
	detector_event_timeline timeline(2);
	add(timeline, 0, {9, 9});
	add(timeline, 1, {0, 1,  2, 3,  4, 5});

	const auto positions = timeline.get_positions(1);

	REQUIRE( positions.get_rank() == 2 );
	REQUIRE( positions.get_event_count() == 3 );
	REQUIRE( positions(2, 0) == 4 );
	REQUIRE( positions(2, 1) == 5 );
	REQUIRE( timeline.get_positions().get_event_count() == 4 );
}

TEST_CASE(
	"a detector_event_timeline keeps its rank and capacity when cleared",
	"[detector_event_timeline]" )
{
	detector_event_timeline timeline(2);
	timeline.reserve(4, 16);
	add(timeline, 7, {1, 2, 3, 4});
	const auto *data = timeline.get_positions().get_coordinates().data();

	timeline.clear();

	REQUIRE( timeline.get_rank() == 2 );
	REQUIRE( timeline.get_group_count() == 0 );
	REQUIRE( timeline.get_event_count() == 0 );

	SECTION( "an earlier timestamp is taken after clearing" )
	{
		add(timeline, 1, {5, 6});

		REQUIRE( timeline.get_timestamp(0) == 1 );
		REQUIRE( to_vector(timeline.get_positions(0)) == coordinates{5, 6} );
	}

	SECTION( "what fits the capacity is not allocated again" )
	{
		add(timeline, 0, coordinates(32, 1));

		REQUIRE( timeline.get_positions().get_coordinates().data() == data );
	}
}

TEST_CASE(
	"a detector_event_timeline copies and moves",
	"[detector_event_timeline]" )
{
	detector_event_timeline original(1);
	add(original, 2, {8, 9});

	detector_event_timeline copy(original);
	REQUIRE( copy.get_timestamp(0) == 2 );
	REQUIRE( to_vector(copy.get_positions(0)) == coordinates{8, 9} );

	const detector_event_timeline moved(std::move(copy));
	REQUIRE( moved.get_rank() == 1 );
	REQUIRE( to_vector(moved.get_positions(0)) == coordinates{8, 9} );

	detector_event_timeline assigned(1);
	assigned = original;
	REQUIRE( to_vector(assigned.get_positions()) == coordinates{8, 9} );
}
