// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `ImageLocationGrouping` to a module.
 *
 * @param m The module.
 */
void bind_image_location_grouping(nanobind::module_ &m);

} // namespace vitrio
