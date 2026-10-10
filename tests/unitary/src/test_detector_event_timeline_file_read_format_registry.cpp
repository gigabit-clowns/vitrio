// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/detector_event_timeline_file_read_format_registry.hpp>

#include "mock/mock_detector_event_timeline_file_read_format.hpp"
#include "mock/mock_image_file_format_factory.hpp"

#include <vitrio/detector_event_timeline_file_read_format.hpp>
#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>
#include <vitrio/image_file_format_suitability.hpp>
#include <vitrio/image_file_probe.hpp>

#include <memory>
#include <trompeloeil.hpp>
#include <utility>

using namespace vitrio;

TEST_CASE(
	"a detector event format registry hands its formats to a selector",
	"[detector_event_timeline_file_read_format_registry]" )
{
	using factory = mock_image_file_format_factory<
		detector_event_timeline_file_read_format
	>;

	detector_event_timeline_file_read_format_registry registry;
	// Declared before the expectations, so the formats it owns outlive them.
	detector_event_timeline_file_read_format_selector selector;
	const image_file_probe probe("absent.eer");

	SECTION( "a drained registry hands the selector what its factory makes" )
	{
		auto format =
			std::make_unique<mock_detector_event_timeline_file_read_format>();
		const auto *expected = format.get();

		ALLOW_CALL(*format, get_suitability(ANY(const image_file_probe&)))
			.RETURN(image_file_format_suitability::normal);
		REQUIRE_CALL(factory::get_instance(), make())
			.LR_RETURN(std::move(format));

		registry.add(&factory::create);
		registry.register_all(selector);

		REQUIRE( selector.get_most_suitable_format(probe) == expected );
	}

	SECTION( "a null factory is ignored" )
	{
		registry.add(nullptr);
		registry.register_all(selector);

		REQUIRE( selector.get_most_suitable_format(probe) == nullptr );
	}
}
