// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `DetectorEventTimelineReader` to a module.
 *
 * @param m The module.
 */
void bind_detector_event_timeline_reader(nanobind::module_ &m);

} // namespace vitrio
