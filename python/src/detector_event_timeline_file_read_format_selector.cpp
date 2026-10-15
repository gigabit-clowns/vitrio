// SPDX-License-Identifier: LGPL-2.1-or-later

#include "detector_event_timeline_file_read_format_selector.hpp"

#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>

#include <nanobind/stl/shared_ptr.h>

namespace vitrio
{

namespace nb = nanobind;

void bind_detector_event_timeline_file_read_format_selector(nb::module_ &m)
{
	nb::class_<detector_event_timeline_file_read_format_selector>(
		m,
		"DetectorEventTimelineFileReadFormatSelector",
		"Holds the formats the events of a detector may be read from."
	)
		.def_static(
			"get_shared",
			&detector_event_timeline_file_read_format_selector::get_shared,
			"Get the selector that holds the formats that come with vitrio.\n"
			"\n"
			"Returns:\n"
			"    DetectorEventTimelineFileReadFormatSelector: Holds EER, when "
			"built with TIFF."
		);
}

} // namespace vitrio
