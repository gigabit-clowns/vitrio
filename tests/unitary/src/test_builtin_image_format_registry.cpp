// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <builtin_image_format_registry.hpp>

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
