// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add the functions that create a scratch to a module.
 *
 * @param m The module.
 */
void bind_indexed_image_scratch(nanobind::module_ &m);

} // namespace vitrio
