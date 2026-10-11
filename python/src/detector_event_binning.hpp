// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `DetectorEventBinning` to a module.
 *
 * @param m The module.
 */
void bind_detector_event_binning(nanobind::module_ &m);

} // namespace vitrio
