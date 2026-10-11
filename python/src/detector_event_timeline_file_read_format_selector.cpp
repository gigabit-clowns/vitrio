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
		"DetectorEventTimelineFileReadFormatSelector"
	)
		.def_static(
			"get_shared",
			&detector_event_timeline_file_read_format_selector::get_shared
		);
}

} // namespace vitrio
