// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `NumericalType` to a module.
 *
 * @param m The module.
 */
void bind_numerical_type(nanobind::module_ &m);

} // namespace vitrio
