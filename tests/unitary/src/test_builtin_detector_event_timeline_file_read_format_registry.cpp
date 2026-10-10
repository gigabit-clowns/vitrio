// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <builtin_detector_event_timeline_file_read_format_registry.hpp>

using namespace vitrio;

TEST_CASE(
	"the built-in detector event format registry is one instance",
	"[builtin_detector_event_timeline_file_read_format_registry]" )
{
	REQUIRE(
		&get_builtin_detector_event_timeline_file_read_format_registry() ==
		&get_builtin_detector_event_timeline_file_read_format_registry()
	);
}
