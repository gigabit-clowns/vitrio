// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `DetectorEventTimelineFileReadFormatSelector` to a module.
 *
 * @param m The module.
 */
void bind_detector_event_timeline_file_read_format_selector(
	nanobind::module_ &m
);

} // namespace vitrio
