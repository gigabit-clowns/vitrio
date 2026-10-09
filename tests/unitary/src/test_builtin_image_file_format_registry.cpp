// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <builtin_image_file_format_registry.hpp>

#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_file_read_format.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_file_write_format.hpp>
#include <vitrio/image_file_write_format_selector.hpp>
#include <vitrio/tests/assets.hpp>

using namespace vitrio;

TEST_CASE(
	"each built-in image format registry is one instance",
	"[builtin_image_file_format_registry]"
)
{
	REQUIRE( &get_builtin_image_file_read_format_registry() ==
		&get_builtin_image_file_read_format_registry() );
	REQUIRE( &get_builtin_image_file_write_format_registry() ==
		&get_builtin_image_file_write_format_registry() );
}

TEST_CASE(
	"the built-in image format registries hold the bundled formats",
	"[builtin_image_file_format_registry]"
)
{
	SECTION( "draining the read registry lets a selector read an MRC file" )
	{
		image_file_read_format_selector selector;
		get_builtin_image_file_read_format_registry().register_all(selector);

		const auto *chosen = selector.get_most_suitable_format(
			image_file_probe(get_mrc_asset_path("EMD-3197.map")));

		REQUIRE( chosen != nullptr );
		REQUIRE( chosen->get_name() == "MRC" );
	}

	SECTION( "draining the write registry lets a selector create an MRC file" )
	{
		image_file_write_format_selector selector;
		get_builtin_image_file_write_format_registry().register_all(selector);

		const auto *chosen = selector.get_most_suitable_format(
			image_file_probe("absent.mrc"));

		REQUIRE( chosen != nullptr );
		REQUIRE( chosen->get_name() == "MRC" );
	}
}
