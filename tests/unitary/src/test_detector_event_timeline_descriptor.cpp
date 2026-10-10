// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/detector_event_timeline_descriptor.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace vitrio;

namespace
{

using extents = std::vector<std::size_t>;

std::vector<std::size_t> to_vector(span<const std::size_t> values)
{
	return std::vector<std::size_t>(values.begin(), values.end());
}

detector_event_timeline_descriptor describe(
	const extents &grid,
	const extents &subdivisions,
	std::uint64_t time_extent = 10,
	double time_quantum = 0.0
)
{
	return detector_event_timeline_descriptor(
		make_span(grid),
		make_span(subdivisions),
		time_extent,
		time_quantum
	);
}

} // anonymous namespace

TEST_CASE(
	"a detector_event_timeline_descriptor holds what it was given",
	"[detector_event_timeline_descriptor]" )
{
	const auto descriptor = describe({16, 32}, {4, 4}, 1000, 0.004);

	REQUIRE( descriptor.get_rank() == 2 );
	REQUIRE( to_vector(descriptor.get_extents()) == extents{16, 32} );
	REQUIRE( to_vector(descriptor.get_subdivisions()) == extents{4, 4} );
	REQUIRE( descriptor.get_time_extent() == 1000 );
	REQUIRE( descriptor.get_time_quantum() == 0.004 );
}

TEST_CASE(
	"a detector_event_timeline_descriptor accepts what bounds a grid",
	"[detector_event_timeline_descriptor]" )
{
	SECTION( "an extent of 2^32" )
	{
		const std::size_t largest =
			static_cast<std::size_t>(std::uint64_t(1) << 32);
		if (largest != 0)
		{
			REQUIRE_NOTHROW( describe({largest}, {1}) );
		}
	}

	SECTION( "a single axis" )
	{
		REQUIRE_NOTHROW( describe({7}, {7}) );
	}

	SECTION( "an unknown time quantum" )
	{
		REQUIRE( describe({4}, {1}).get_time_quantum() == 0.0 );
	}
}

TEST_CASE(
	"a detector_event_timeline_descriptor refuses what describes no grid",
	"[detector_event_timeline_descriptor]" )
{
	SECTION( "no extents" )
	{
		REQUIRE_THROWS_AS( describe({}, {}), std::invalid_argument );
	}

	SECTION( "subdivisions of another rank" )
	{
		REQUIRE_THROWS_AS( describe({4, 4}, {1}), std::invalid_argument );
	}

	SECTION( "a zero extent" )
	{
		REQUIRE_THROWS_AS( describe({0, 4}, {1, 1}), std::invalid_argument );
	}

	SECTION( "a zero subdivision" )
	{
		REQUIRE_THROWS_AS( describe({4, 4}, {0, 1}), std::invalid_argument );
	}

	SECTION( "a subdivision that does not divide its extent" )
	{
		REQUIRE_THROWS_AS( describe({4, 6}, {2, 4}), std::invalid_argument );
	}

	SECTION( "an extent past 2^32" )
	{
		const auto beyond = (std::uint64_t(1) << 32) + 1;
		if (beyond <= std::numeric_limits<std::size_t>::max())
		{
			REQUIRE_THROWS_AS(
				describe({static_cast<std::size_t>(beyond)}, {1}),
				std::invalid_argument
			);
		}
	}

	SECTION( "no time" )
	{
		REQUIRE_THROWS_AS( describe({4}, {1}, 0), std::invalid_argument );
	}

	SECTION( "a negative time quantum" )
	{
		REQUIRE_THROWS_AS(
			describe({4}, {1}, 1, -1.0),
			std::invalid_argument
		);
	}

	SECTION( "a time quantum that is not finite" )
	{
		REQUIRE_THROWS_AS(
			describe({4}, {1}, 1, std::numeric_limits<double>::infinity()),
			std::invalid_argument
		);
		REQUIRE_THROWS_AS(
			describe({4}, {1}, 1, std::numeric_limits<double>::quiet_NaN()),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"detector_event_timeline_descriptors compare by every component",
	"[detector_event_timeline_descriptor]" )
{
	const auto descriptor = describe({8, 8}, {2, 2}, 5, 1.0);

	REQUIRE( descriptor == describe({8, 8}, {2, 2}, 5, 1.0) );
	REQUIRE( descriptor != describe({8, 4}, {2, 2}, 5, 1.0) );
	REQUIRE( descriptor != describe({8, 8}, {1, 2}, 5, 1.0) );
	REQUIRE( descriptor != describe({8, 8}, {2, 2}, 6, 1.0) );
	REQUIRE( descriptor != describe({8, 8}, {2, 2}, 5, 2.0) );
}

TEST_CASE(
	"a detector_event_timeline_descriptor copies and moves",
	"[detector_event_timeline_descriptor]" )
{
	const auto original = describe({8, 8}, {2, 2}, 5, 1.0);

	auto copy = original;
	REQUIRE( copy == original );

	const auto moved = std::move(copy);
	REQUIRE( moved == original );

	auto assigned = describe({1}, {1});
	assigned = original;
	REQUIRE( assigned == original );
}
