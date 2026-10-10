// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/counting_detector_event_renderer.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/detector_event_timeline.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/tests/host_array.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
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
		make_span(grid),
		make_span(subdivisions),
		10,
		0.0
	);
}

std::unique_ptr<counting_detector_event_renderer> make_renderer(
	const extents &bin
)
{
	return std::make_unique<counting_detector_event_renderer>(make_span(bin));
}

detector_event_timeline make_events(
	std::size_t rank,
	const std::vector<std::uint32_t> &first,
	const std::vector<std::uint32_t> &second
)
{
	detector_event_timeline events(rank);
	events.add_group(0, make_span(first));
	events.add_group(1, make_span(second));
	return events;
}

template <typename T>
std::vector<T> read_back(const array &image, std::size_t count)
{
	const auto *values = reinterpret_cast<const T*>(image.get_data());
	return std::vector<T>(values, values + count);
}

} // anonymous namespace

TEST_CASE(
	"a counting_detector_event_renderer holds its bin",
	"[counting_detector_event_renderer]" )
{
	const auto renderer = make_renderer({2, 4});

	const auto bin = renderer->get_bin_extents();
	REQUIRE( extents(bin.begin(), bin.end()) == extents{2, 4} );
	REQUIRE( renderer->get_data_type() == numerical_type::uint32 );
}

TEST_CASE(
	"a counting_detector_event_renderer refuses an empty bin",
	"[counting_detector_event_renderer]" )
{
	REQUIRE_THROWS_AS( make_renderer({}), std::invalid_argument );
	REQUIRE_THROWS_AS( make_renderer({2, 0}), std::invalid_argument );
}

TEST_CASE(
	"a counting_detector_event_renderer divides the grid into its bins",
	"[counting_detector_event_renderer]" )
{
	SECTION( "a bin that divides the grid" )
	{
		REQUIRE( make_renderer({4, 4})->get_image_extents(
			describe({16, 32}, {4, 4})) == extents{4, 8} );
	}

	SECTION( "a bin finer than a pixel of the detector" )
	{
		REQUIRE( make_renderer({1, 1})->get_image_extents(
			describe({16, 32}, {4, 4})) == extents{16, 32} );
	}

	SECTION( "a bin that does not divide the grid" )
	{
		REQUIRE( make_renderer({2, 3})->get_image_extents(
			describe({5, 7}, {1, 1})) == extents{3, 3} );
	}

	SECTION( "a grid of another rank" )
	{
		REQUIRE_THROWS_AS(
			make_renderer({2})->get_image_extents(describe({4, 4}, {1, 1})),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"a counting_detector_event_renderer counts the events of each pixel",
	"[counting_detector_event_renderer]" )
{
	// A grid of 4 by 6 quanta, binned by 2 into 2 by 3 pixels.
	const auto descriptor = describe({4, 6}, {2, 2});
	const auto renderer = make_renderer({2, 2});
	const auto events = make_events(
		2,
		{0, 0,  1, 1,  0, 5},
		{3, 2,  2, 3,  1, 0,  3, 5}
	);

	SECTION( "in the data type it counts in" )
	{
		auto image = test::make_host_array<std::uint32_t>(
			{2, 3}, numerical_type::uint32, 99);

		renderer->render(descriptor, events, image);

		REQUIRE( read_back<std::uint32_t>(image, 6) ==
			std::vector<std::uint32_t>{3, 0, 1,  0, 2, 1} );
	}

	SECTION( "converted to another data type" )
	{
		auto image = test::make_host_array<float>(
			{2, 3}, numerical_type::float32, -1.0f);

		renderer->render(descriptor, events, image);

		REQUIRE( read_back<float>(image, 6) ==
			std::vector<float>{3, 0, 1,  0, 2, 1} );
	}

	SECTION( "into a strided destination" )
	{
		std::vector<std::uint16_t> memory(16, 7);
		const array_descriptor strided(
			{2, 3}, {8, 2}, 1, numerical_type::uint16);
		const array_ref image(
			make_span(
				reinterpret_cast<byte*>(memory.data()),
				memory.size() * sizeof(std::uint16_t)
			),
			strided
		);

		renderer->render(descriptor, events, image);

		REQUIRE( memory == std::vector<std::uint16_t>{
			7, 3, 7, 0, 7, 1, 7, 7,
			7, 0, 7, 2, 7, 1, 7, 7
		} );
	}

	SECTION( "with no events" )
	{
		auto image = test::make_host_array<std::uint32_t>(
			{2, 3}, numerical_type::uint32, 99);

		renderer->render(descriptor, detector_event_timeline(2), image);

		REQUIRE( read_back<std::uint32_t>(image, 6) ==
			std::vector<std::uint32_t>(6, 0) );
	}
}

TEST_CASE(
	"a counting_detector_event_renderer gives the last pixel what is left",
	"[counting_detector_event_renderer]" )
{
	const auto descriptor = describe({5}, {1});
	const auto renderer = make_renderer({2});
	const auto events = make_events(1, {0, 1, 2}, {3, 4, 4});
	auto image = test::make_host_array<std::uint32_t>(
		{3}, numerical_type::uint32);

	renderer->render(descriptor, events, image);

	REQUIRE( read_back<std::uint32_t>(image, 3) ==
		std::vector<std::uint32_t>{2, 2, 2} );
}

TEST_CASE(
	"a counting_detector_event_renderer refuses what it can not render",
	"[counting_detector_event_renderer]" )
{
	const auto descriptor = describe({4, 6}, {2, 2});
	const auto renderer = make_renderer({2, 2});
	auto image = test::make_host_array<std::uint32_t>(
		{2, 3}, numerical_type::uint32);

	SECTION( "events of another rank" )
	{
		REQUIRE_THROWS_AS(
			renderer->render(descriptor, detector_event_timeline(1), image),
			std::invalid_argument
		);
	}

	SECTION( "a destination of other extents" )
	{
		auto other = test::make_host_array<std::uint32_t>(
			{3, 2}, numerical_type::uint32);

		REQUIRE_THROWS_AS(
			renderer->render(descriptor, detector_event_timeline(2), other),
			std::invalid_argument
		);
	}

	SECTION( "a destination that is not initialized" )
	{
		REQUIRE_THROWS_AS(
			renderer->render(
				descriptor, detector_event_timeline(2), array_ref()),
			std::invalid_argument
		);
	}

	SECTION( "an event outside the grid" )
	{
		const auto events = make_events(2, {0, 0}, {4, 0});

		REQUIRE_THROWS_AS(
			renderer->render(descriptor, events, image),
			std::out_of_range
		);
	}
}
