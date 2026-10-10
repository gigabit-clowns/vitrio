// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/detector_event_fractionation.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace vitrio;

namespace
{

std::vector<std::uint64_t> get_lengths(
	const detector_event_fractionation &fractionation,
	std::uint64_t time_extent
)
{
	std::vector<std::uint64_t> lengths;
	for (std::size_t i = 0; i < fractionation.get_fraction_count(); ++i)
	{
		lengths.push_back(
			fractionation.get_fraction_begin(i + 1, time_extent) -
			fractionation.get_fraction_begin(i, time_extent)
		);
	}
	return lengths;
}

} // anonymous namespace

TEST_CASE(
	"a detector_event_fractionation holds its fraction count",
	"[detector_event_fractionation]" )
{
	const detector_event_fractionation fractionation(40);

	REQUIRE( fractionation.get_fraction_count() == 40 );
	REQUIRE( fractionation == detector_event_fractionation(40) );
	REQUIRE( fractionation != detector_event_fractionation(41) );
}

TEST_CASE(
	"a detector_event_fractionation refuses fraction counts out of range",
	"[detector_event_fractionation]" )
{
	REQUIRE_THROWS_AS(
		detector_event_fractionation(0),
		std::invalid_argument
	);

	const auto largest = detector_event_fractionation::max_fraction_count;
	REQUIRE_NOTHROW( detector_event_fractionation(largest) );
	if (largest < std::numeric_limits<std::size_t>::max())
	{
		REQUIRE_THROWS_AS(
			detector_event_fractionation(largest + 1),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"a detector_event_fractionation covers the whole time",
	"[detector_event_fractionation]" )
{
	const detector_event_fractionation fractionation(7);

	REQUIRE( fractionation.get_fraction_begin(0, 100) == 0 );
	REQUIRE( fractionation.get_fraction_begin(7, 100) == 100 );
}

TEST_CASE(
	"a detector_event_fractionation cuts time as evenly as it can",
	"[detector_event_fractionation]" )
{
	SECTION( "a time the fractions divide" )
	{
		const detector_event_fractionation fractionation(4);

		REQUIRE( get_lengths(fractionation, 12) ==
			std::vector<std::uint64_t>{3, 3, 3, 3} );
	}

	SECTION( "the longer fractions are spread over the time" )
	{
		const detector_event_fractionation fractionation(4);

		REQUIRE( get_lengths(fractionation, 14) ==
			std::vector<std::uint64_t>{3, 4, 3, 4} );
	}

	SECTION( "as many fractions as time quanta" )
	{
		const detector_event_fractionation fractionation(5);

		REQUIRE( get_lengths(fractionation, 5) ==
			std::vector<std::uint64_t>{1, 1, 1, 1, 1} );
	}

	SECTION( "a single fraction spans everything" )
	{
		const detector_event_fractionation fractionation(1);

		REQUIRE( get_lengths(fractionation, 1234) ==
			std::vector<std::uint64_t>{1234} );
	}

	SECTION( "lengths differ by one at most" )
	{
		const detector_event_fractionation fractionation(37);
		for (std::uint64_t time = 37; time < 1000; time += 13)
		{
			const auto lengths = get_lengths(fractionation, time);
			std::uint64_t total = 0;
			for (const auto length : lengths)
			{
				REQUIRE( (length == time / 37 || length == time / 37 + 1) );
				total += length;
			}
			REQUIRE( total == time );
		}
	}
}

TEST_CASE(
	"a detector_event_fractionation does not overflow on long times",
	"[detector_event_fractionation]" )
{
	const auto time = std::numeric_limits<std::uint64_t>::max();
	const auto count = detector_event_fractionation::max_fraction_count;
	const detector_event_fractionation fractionation(count);

	REQUIRE( fractionation.get_fraction_begin(count, time) == time );

	// T = 2^64-1 = (2^32-1)(2^32+1), so every fraction spans 2^32+1.
	const std::uint64_t length = (std::uint64_t(1) << 32) + 1;
	REQUIRE( fractionation.get_fraction_begin(1, time) == length );
	REQUIRE( fractionation.get_fraction_begin(count - 1, time) ==
		time - length );
}
