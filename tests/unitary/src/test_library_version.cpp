// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/library_version.hpp>

using namespace vitrio;

TEST_CASE(
	"get_library_version reports the version of the project",
	"[library_version]"
)
{
	const version expected(
		VITRIO_TEST_VERSION_MAJOR,
		VITRIO_TEST_VERSION_MINOR,
		VITRIO_TEST_VERSION_PATCH
	);

	CHECK( get_library_version() == expected );
}
