// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_format_registration.hpp>

#include "mock/mock_image_format_registry.hpp"
#include "mock/mock_image_read_format.hpp"
#include "mock/mock_image_write_format.hpp"

#include <vitrio/image_read_format.hpp>
#include <vitrio/image_write_format.hpp>

#include <trompeloeil.hpp>

using namespace vitrio;

TEST_CASE(
	"a registration appends a factory for its format to a registry",
	"[image_format_registration]"
)
{
	SECTION( "a read format" )
	{
		using registry_type = mock_image_format_registry<image_read_format>;
		registry_type registry;

		REQUIRE_CALL(registry, add(trompeloeil::_))
			.WITH(
				dynamic_cast<const mock_image_read_format*>(_1().get()) !=
				nullptr
			);

		const image_format_registration<
			mock_image_read_format,
			registry_type
		> registration(registry);
	}

	SECTION( "a write format" )
	{
		using registry_type = mock_image_format_registry<image_write_format>;
		registry_type registry;

		REQUIRE_CALL(registry, add(trompeloeil::_))
			.WITH(
				dynamic_cast<const mock_image_write_format*>(_1().get()) !=
				nullptr
			);

		const image_format_registration<
			mock_image_write_format,
			registry_type
		> registration(registry);
	}
}
