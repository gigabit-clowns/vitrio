// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/detector_event_binning.hpp>

#include <vitrio/detector_event_timeline_descriptor.hpp>

#include <cstddef>
#include <stdexcept>
#include <vector>

using namespace vitrio;

namespace
{

using extents = std::vector<std::size_t>;

detector_event_timeline_descriptor describe(
	const extents &grid,
	const extents &subdivisions
)
{
	return detector_event_timeline_descriptor(
		make_span(grid), make_span(subdivisions), 1, 0.0);
}

} // anonymous namespace

TEST_CASE( "a detector_event_binning holds its factor",
	"[detector_event_binning]" )
{
	const detector_event_binning binning(2);

	REQUIRE( binning.get_factor() == 2 );
	REQUIRE( binning == detector_event_binning(2) );
	REQUIRE( binning != detector_event_binning(4) );
}

TEST_CASE( "a detector_event_binning refuses a factor of zero",
	"[detector_event_binning]" )
{
	REQUIRE_THROWS_AS( detector_event_binning(0), std::invalid_argument );
}

TEST_CASE( "a detector_event_binning resolves into position quanta",
	"[detector_event_binning]" )
{
	SECTION( "a factor of one gathers a pixel of the detector" )
	{
		REQUIRE( detector_event_binning(1).resolve(
			describe({16, 32}, {4, 4})) == extents{4, 4} );
	}

	SECTION( "a larger factor gathers blocks of pixels" )
	{
		REQUIRE( detector_event_binning(2).resolve(
			describe({16, 32}, {4, 4})) == extents{8, 8} );
	}

	SECTION( "each axis keeps its own subdivisions" )
	{
		REQUIRE( detector_event_binning(2).resolve(
			describe({8, 4}, {2, 1})) == extents{4, 2} );
	}

	SECTION( "any rank is resolved" )
	{
		REQUIRE( detector_event_binning(3).resolve(
			describe({9}, {1})) == extents{3} );
	}

	SECTION( "a factor that does not divide the pixels is refused" )
	{
		// 24 quanta of 4 make 6 pixels, which 4 does not divide.
		REQUIRE_THROWS_AS(
			detector_event_binning(4).resolve(describe({24, 32}, {4, 4})),
			std::invalid_argument
		);
	}
}
