// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `DetectorEventRenderer` to a module.
 *
 * @param m The module.
 */
void bind_detector_event_renderer(nanobind::module_ &m);

} // namespace vitrio
