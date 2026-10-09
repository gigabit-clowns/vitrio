// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <builtin_image_format_registry.hpp>

#include <vitrio/image_probe.hpp>
#include <vitrio/image_read_format.hpp>
#include <vitrio/image_read_format_manager.hpp>
#include <vitrio/image_write_format.hpp>
#include <vitrio/image_write_format_manager.hpp>
#include <vitrio/tests/assets.hpp>

using namespace vitrio;

TEST_CASE(
	"each built-in image format registry is one instance",
	"[builtin_image_format_registry]"
)
{
	REQUIRE( &get_builtin_image_read_format_registry() ==
		&get_builtin_image_read_format_registry() );
	REQUIRE( &get_builtin_image_write_format_registry() ==
		&get_builtin_image_write_format_registry() );
}

TEST_CASE(
	"the built-in image format registries hold the bundled formats",
	"[builtin_image_format_registry]"
)
{
	SECTION( "draining the read registry lets a manager read an MRC file" )
	{
		image_read_format_manager manager;
		get_builtin_image_read_format_registry().register_all(manager);

		const auto *chosen = manager.get_most_suitable_format(
			image_probe(get_mrc_asset_path("EMD-3197.map")));

		REQUIRE( chosen != nullptr );
		REQUIRE( chosen->get_name() == "MRC" );
	}

	SECTION( "draining the write registry lets a manager create an MRC file" )
	{
		image_write_format_manager manager;
		get_builtin_image_write_format_registry().register_all(manager);

		const auto *chosen = manager.get_most_suitable_format(
			image_probe("absent.mrc"));

		REQUIRE( chosen != nullptr );
		REQUIRE( chosen->get_name() == "MRC" );
	}
}
