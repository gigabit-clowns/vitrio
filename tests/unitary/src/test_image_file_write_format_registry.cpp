// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_file_write_format_registry.hpp>

#include "mock/mock_image_file_format_factory.hpp"
#include "mock/mock_image_file_write_format.hpp"

#include <vitrio/image_file_format_suitability.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_file_write_format.hpp>
#include <vitrio/image_file_write_format_selector.hpp>

#include <memory>
#include <trompeloeil.hpp>
#include <utility>

using namespace vitrio;

TEST_CASE(
	"a write registry hands its formats to a selector",
	"[image_file_write_format_registry]"
)
{
	using factory = mock_image_file_format_factory<image_file_write_format>;

	image_file_write_format_registry registry;
	// Declared before the expectations, so the formats it owns outlive them.
	image_file_write_format_selector selector;
	const image_file_probe probe("absent.mrc");

	SECTION( "a drained registry hands the selector what its factory makes" )
	{
		auto format = std::make_unique<mock_image_file_write_format>();
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

	SECTION( "an empty registry registers nothing" )
	{
		registry.register_all(selector);

		REQUIRE( selector.get_most_suitable_format(probe) == nullptr );
	}
}
