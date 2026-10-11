// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/counting_detector_event_renderer.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/detector_event_binning.hpp>
#include <vitrio/detector_event_position_view.hpp>
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
	std::size_t factor
)
{
	return std::make_unique<counting_detector_event_renderer>(
		detector_event_binning(factor));
}

detector_event_timeline make_events(
	std::size_t rank,
	const std::vector<std::uint32_t> &first,
	const std::vector<std::uint32_t> &second
)
{
	detector_event_timeline events(rank);
	events.add_group(0, detector_event_position_view(make_span(first), rank));
	events.add_group(1, detector_event_position_view(make_span(second), rank));
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
	"a counting_detector_event_renderer holds its binning",
	"[counting_detector_event_renderer]" )
{
	const auto renderer = make_renderer(2);

	REQUIRE( renderer->get_binning() == detector_event_binning(2) );
	REQUIRE( renderer->get_data_type() == numerical_type::uint32 );
}

TEST_CASE(
	"a counting_detector_event_renderer bins the pixels of the detector",
	"[counting_detector_event_renderer]" )
{
	SECTION( "a factor of one renders at the resolution of the detector" )
	{
		REQUIRE( make_renderer(1)->get_image_extents(
			describe({16, 32}, {4, 4})) == extents{4, 8} );
	}

	SECTION( "a larger factor merges blocks of pixels" )
	{
		REQUIRE( make_renderer(2)->get_image_extents(
			describe({16, 32}, {4, 4})) == extents{2, 4} );
		REQUIRE( make_renderer(4)->get_image_extents(
			describe({8, 8}, {1, 1})) == extents{2, 2} );
	}

	SECTION( "a factor that does not divide the pixels is refused" )
	{
		REQUIRE_THROWS_AS(
			make_renderer(2)->get_image_extents(describe({6, 8}, {2, 2})),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"a counting_detector_event_renderer counts the events of each pixel",
	"[counting_detector_event_renderer]" )
{
	// A grid of 4 by 6 quanta, which subdivisions of 2 make 2 by 3 pixels.
	const auto descriptor = describe({4, 6}, {2, 2});
	const auto renderer = make_renderer(1);
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
	"a counting_detector_event_renderer counts the events of merged pixels",
	"[counting_detector_event_renderer]" )
{
	const auto descriptor = describe({4}, {1});
	const auto renderer = make_renderer(2);
	const auto events = make_events(1, {0, 1, 2}, {3, 3});
	auto image = test::make_host_array<std::uint32_t>(
		{2}, numerical_type::uint32);

	renderer->render(descriptor, events, image);

	REQUIRE( read_back<std::uint32_t>(image, 2) ==
		std::vector<std::uint32_t>{2, 3} );
}

TEST_CASE(
	"a counting_detector_event_renderer refuses what it can not render",
	"[counting_detector_event_renderer]" )
{
	const auto descriptor = describe({4, 6}, {2, 2});
	const auto renderer = make_renderer(1);
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
