// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `ImageDescriptor` and `get_core_extents` to a module.
 *
 * @param m The module.
 */
void bind_image_descriptor(nanobind::module_ &m);

} // namespace vitrio
