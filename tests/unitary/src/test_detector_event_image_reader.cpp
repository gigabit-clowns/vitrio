// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/detector_event_image_reader.hpp>

#include "mock/mock_detector_event_renderer.hpp"
#include "mock/mock_detector_event_timeline_reader.hpp"

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/counting_detector_event_renderer.hpp>
#include <vitrio/detector_event_fractionation.hpp>
#include <vitrio/detector_event_position_view.hpp>
#include <vitrio/detector_event_timeline.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <trompeloeil.hpp>
#include <vector>

using namespace vitrio;

namespace
{

using extents = std::vector<std::size_t>;
using counts = std::vector<std::uint32_t>;

// Five frames on a grid of 2 by 3 pixels, frame t holding a single event at
// (t%2, t%3): (0,0), (1,1), (0,2), (1,0) and (0,1).
const extents grid = {2, 3};
const extents subdivisions = {1, 1};
const std::uint64_t frame_count = 5;

void read_frames(
	std::uint64_t time_begin,
	std::uint64_t time_end,
	detector_event_timeline &destination
)
{
	destination.clear();
	for (auto frame = time_begin; frame < time_end; ++frame)
	{
		const std::array<std::uint32_t, 2> event = {{
			static_cast<std::uint32_t>(frame % 2),
			static_cast<std::uint32_t>(frame % 3)
		}};
		destination.add_group(
			frame, detector_event_position_view(make_span(event), 2));
	}
}

/**
 * The acquisition above, read through a mock and rendered by counting, with
 * every read let through and counted.
 */
class detector_event_image_reader_fixture
{
public:
	detector_event_image_reader_fixture()
		: m_descriptor(
			make_span(grid), make_span(subdivisions), frame_count, 0.0)
		, m_events(std::make_shared<mock_detector_event_timeline_reader>())
		, m_renderer(std::make_shared<counting_detector_event_renderer>(
			make_span(subdivisions)))
		, m_read_count(0)
	{
		m_expectations.push_back(NAMED_ALLOW_CALL(
			*m_events,
			get_descriptor()
		).RETURN(std::ref(m_descriptor)));
		m_expectations.push_back(NAMED_ALLOW_CALL(
			*m_events,
			read(ANY(std::uint64_t), ANY(std::uint64_t),
				ANY(detector_event_timeline&))
		).LR_SIDE_EFFECT(++m_read_count)
			.SIDE_EFFECT(read_frames(_1, _2, _3)));
	}

	std::unique_ptr<detector_event_image_reader> make_reader(
		std::size_t fraction_count
	)
	{
		return std::make_unique<detector_event_image_reader>(
			m_events,
			detector_event_fractionation(fraction_count),
			m_renderer
		);
	}

	std::size_t get_read_count() const noexcept
	{
		return m_read_count;
	}

protected:
	detector_event_timeline_descriptor m_descriptor;
	std::shared_ptr<mock_detector_event_timeline_reader> m_events;
	std::shared_ptr<counting_detector_event_renderer> m_renderer;
	std::size_t m_read_count;
	std::vector<std::unique_ptr<trompeloeil::expectation>> m_expectations;
};

counts read_back(const array &values, std::size_t count)
{
	const auto *data = reinterpret_cast<const std::uint32_t*>(
		values.get_data());
	return counts(data, data + count);
}

} // anonymous namespace

TEST_CASE_METHOD(
	detector_event_image_reader_fixture,
	"a detector_event_image_reader describes a movie of its fractions",
	"[detector_event_image_reader]" )
{
	SECTION( "several fractions are a stack of images" )
	{
		const auto reader = make_reader(2);
		const auto &descriptor = reader->get_descriptor();
		const auto read_extents = descriptor.get_extents();

		REQUIRE( extents(read_extents.begin(), read_extents.end()) ==
			extents{2, 2, 3} );
		REQUIRE( descriptor.get_core_rank() == 2 );
		REQUIRE( descriptor.get_data_type() == numerical_type::uint32 );
		REQUIRE( reader->get_fractionation() ==
			detector_event_fractionation(2) );
	}

	SECTION( "a single fraction is one image" )
	{
		const auto reader = make_reader(1);
		const auto read_extents = reader->get_descriptor().get_extents();

		REQUIRE( extents(read_extents.begin(), read_extents.end()) ==
			extents{2, 3} );
	}

	SECTION( "nothing is read to describe it" )
	{
		make_reader(5);

		REQUIRE( get_read_count() == 0 );
	}
}

TEST_CASE_METHOD(
	detector_event_image_reader_fixture,
	"a detector_event_image_reader renders each fraction it reads",
	"[detector_event_image_reader]" )
{
	SECTION( "the whole of several fractions" )
	{
		// Fraction 0 spans frames 0 and 1, fraction 1 frames 2 to 4.
		const auto reader = make_reader(2);
		auto movie = test::make_host_array<std::uint32_t>(
			{2, 2, 3}, numerical_type::uint32, 99);

		reader->read(movie, test::whole_of({2, 2, 3}));

		REQUIRE( read_back(movie, 12) == counts{
			1, 0, 0,  0, 1, 0,
			0, 1, 1,  1, 0, 0
		} );
		REQUIRE( get_read_count() == 2 );
	}

	SECTION( "a single fraction holds every event" )
	{
		const auto reader = make_reader(1);
		auto image = test::make_host_array<std::uint32_t>(
			{2, 3}, numerical_type::uint32, 99);

		reader->read(image, test::whole_of({2, 3}));

		REQUIRE( read_back(image, 6) == counts{1, 1, 1,  1, 1, 0} );
	}

	SECTION( "a region spanning fractions is cut by fraction" )
	{
		const auto reader = make_reader(2);
		auto patches = test::make_host_array<std::uint32_t>(
			{2, 1, 2}, numerical_type::uint32, 99);
		image_transfer_plan regions(image_transfer_shape({2, 1, 2}, 3, 3));
		const extents file_offset = {0, 1, 1};
		const extents array_offset = {0, 0, 0};
		regions.add(make_span(file_offset), make_span(array_offset));

		reader->read(patches, regions);

		REQUIRE( read_back(patches, 4) == counts{1, 0,  0, 0} );
	}

	SECTION( "a fraction read again is not rendered again" )
	{
		const auto reader = make_reader(5);
		auto image = test::make_host_array<std::uint32_t>(
			{1, 2, 3}, numerical_type::uint32);
		image_transfer_plan regions(image_transfer_shape({1, 2, 3}, 3, 3));
		const extents file_offset = {3, 0, 0};
		const extents array_offset = {0, 0, 0};
		regions.add(make_span(file_offset), make_span(array_offset));

		reader->read(image, regions);
		reader->read(image, regions);

		REQUIRE( get_read_count() == 1 );
		REQUIRE( read_back(image, 6) == counts{0, 0, 0,  1, 0, 0} );
	}

	SECTION( "an empty plan reads nothing" )
	{
		const auto reader = make_reader(2);
		auto image = test::make_host_array<std::uint32_t>(
			{2, 3}, numerical_type::uint32);
		const image_transfer_plan regions(
			image_transfer_shape({2, 3}, 3, 2));

		reader->read(image, regions);

		REQUIRE( get_read_count() == 0 );
	}

	SECTION( "a region outside the movie is refused before anything is read" )
	{
		const auto reader = make_reader(2);
		auto movie = test::make_host_array<std::uint32_t>(
			{2, 2, 3}, numerical_type::uint32);
		image_transfer_plan regions(image_transfer_shape({2, 2, 3}, 3, 3));
		const extents file_offset = {1, 0, 0};
		const extents array_offset = {0, 0, 0};
		regions.add(make_span(file_offset), make_span(array_offset));

		REQUIRE_THROWS_AS( reader->read(movie, regions), std::out_of_range );
		REQUIRE( get_read_count() == 0 );
	}
}

TEST_CASE(
	"a detector_event_image_reader renders again what a failure stopped",
	"[detector_event_image_reader]" )
{
	const detector_event_timeline_descriptor descriptor(
		make_span(grid), make_span(subdivisions), frame_count, 0.0);
	const auto events = std::make_shared<mock_detector_event_timeline_reader>();
	const auto renderer = std::make_shared<counting_detector_event_renderer>(
		make_span(subdivisions));
	ALLOW_CALL(*events, get_descriptor()).RETURN(std::ref(descriptor));

	const detector_event_image_reader reader(
		events, detector_event_fractionation(1), renderer);
	auto image = test::make_host_array<std::uint32_t>(
		{2, 3}, numerical_type::uint32);
	const auto regions = test::whole_of({2, 3});

	{
		REQUIRE_CALL(*events, read(0u, 5u, ANY(detector_event_timeline&)))
			.SIDE_EFFECT( throw std::runtime_error("from the events") );

		REQUIRE_THROWS_AS( reader.read(image, regions), std::runtime_error );
	}

	REQUIRE_CALL(*events, read(0u, 5u, ANY(detector_event_timeline&)))
		.SIDE_EFFECT( read_frames(_1, _2, _3) );

	reader.read(image, regions);

	REQUIRE( read_back(image, 6) == counts{1, 1, 1,  1, 1, 0} );
}

TEST_CASE(
	"a detector_event_image_reader refuses what it can not render",
	"[detector_event_image_reader]" )
{
	const detector_event_timeline_descriptor descriptor(
		make_span(grid), make_span(subdivisions), frame_count, 0.0);
	const auto events = std::make_shared<mock_detector_event_timeline_reader>();
	ALLOW_CALL(*events, get_descriptor()).RETURN(std::ref(descriptor));
	const auto counting = std::make_shared<counting_detector_event_renderer>(
		make_span(subdivisions));

	SECTION( "no events" )
	{
		REQUIRE_THROWS_AS(
			detector_event_image_reader(
				nullptr, detector_event_fractionation(1), counting),
			std::invalid_argument
		);
	}

	SECTION( "no renderer" )
	{
		REQUIRE_THROWS_AS(
			detector_event_image_reader(
				events, detector_event_fractionation(1), nullptr),
			std::invalid_argument
		);
	}

	SECTION( "more fractions than frames" )
	{
		REQUIRE_THROWS_AS(
			detector_event_image_reader(
				events, detector_event_fractionation(6), counting),
			std::invalid_argument
		);
	}

	SECTION( "images of a rank other than two" )
	{
		const auto renderer = std::make_shared<mock_detector_event_renderer>();
		ALLOW_CALL(
			*renderer,
			get_image_extents(ANY(const detector_event_timeline_descriptor&))
		).RETURN(extents{2, 3, 4});
		ALLOW_CALL(*renderer, get_data_type())
			.RETURN(numerical_type::uint32);

		REQUIRE_THROWS_AS(
			detector_event_image_reader(
				events, detector_event_fractionation(1), renderer),
			std::invalid_argument
		);
	}
}
