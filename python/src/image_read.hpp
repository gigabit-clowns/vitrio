// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add the functions that read images to a module.
 *
 * @param m The module.
 */
void bind_image_read(nanobind::module_ &m);

} // namespace vitrio
