// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/detector_event_image_reader_provider.hpp>

#include "mock/mock_detector_event_renderer.hpp"
#include "mock/mock_detector_event_timeline_reader.hpp"
#include "mock/mock_detector_event_timeline_reader_provider.hpp"

#include <vitrio/detector_event_fractionation.hpp>
#include <vitrio/detector_event_image_reader.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/image_descriptor.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <trompeloeil.hpp>
#include <vector>

using namespace vitrio;

namespace
{

using extents = std::vector<std::size_t>;

} // anonymous namespace

TEST_CASE(
	"a detector_event_image_reader_provider needs what it renders with",
	"[detector_event_image_reader_provider]" )
{
	const auto events =
		std::make_shared<mock_detector_event_timeline_reader_provider>();
	const auto renderer = std::make_shared<mock_detector_event_renderer>();

	REQUIRE_THROWS_AS(
		detector_event_image_reader_provider(
			nullptr, detector_event_fractionation(1), renderer),
		std::invalid_argument
	);
	REQUIRE_THROWS_AS(
		detector_event_image_reader_provider(
			events, detector_event_fractionation(1), nullptr),
		std::invalid_argument
	);
}

TEST_CASE(
	"a detector_event_image_reader_provider renders what its key names",
	"[detector_event_image_reader_provider]" )
{
	const extents grid = {4, 4};
	const extents subdivisions = {1, 1};
	const detector_event_timeline_descriptor descriptor(
		make_span(grid), make_span(subdivisions), 10, 0.0);

	const auto events =
		std::make_shared<mock_detector_event_timeline_reader_provider>();
	const auto timeline =
		std::make_shared<mock_detector_event_timeline_reader>();
	const auto renderer = std::make_shared<mock_detector_event_renderer>();
	ALLOW_CALL(*timeline, get_descriptor()).RETURN(std::ref(descriptor));
	ALLOW_CALL(
		*renderer,
		get_image_extents(ANY(const detector_event_timeline_descriptor&))
	).RETURN(extents{2, 2});
	ALLOW_CALL(*renderer, get_data_type()).RETURN(numerical_type::float32);

	detector_event_image_reader_provider provider(
		events, detector_event_fractionation(5), renderer);

	SECTION( "the key is handed to the provider of the events" )
	{
		REQUIRE_CALL(*events, acquire("movie.eer")).RETURN(timeline);

		const auto reader = provider.acquire("movie.eer");
		const auto *rendering =
			dynamic_cast<const detector_event_image_reader*>(reader.get());

		REQUIRE( rendering != nullptr );
		REQUIRE( rendering->get_fractionation() ==
			detector_event_fractionation(5) );

		const auto read_extents = reader->get_descriptor().get_extents();
		REQUIRE( extents(read_extents.begin(), read_extents.end()) ==
			extents{5, 2, 2} );
		REQUIRE( reader->get_descriptor().get_data_type() ==
			numerical_type::float32 );
	}

	SECTION( "a key asked for twice is made twice" )
	{
		REQUIRE_CALL(*events, acquire("movie.eer"))
			.TIMES(2)
			.RETURN(timeline);

		REQUIRE( provider.acquire("movie.eer") !=
			provider.acquire("movie.eer") );
	}

	SECTION( "what the provider of the events reports is reported" )
	{
		REQUIRE_CALL(*events, acquire("missing.eer"))
			.THROW( std::runtime_error("from the events") );

		REQUIRE_THROWS_AS(
			provider.acquire("missing.eer"),
			std::runtime_error
		);
	}
}
