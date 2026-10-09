// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add the functions that write images to a module.
 *
 * @param m The module.
 */
void bind_image_write(nanobind::module_ &m);

} // namespace vitrio
