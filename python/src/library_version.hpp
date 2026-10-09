// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `get_library_version` to a module.
 *
 * @param m The module.
 */
void bind_library_version(nanobind::module_ &m);

} // namespace vitrio
