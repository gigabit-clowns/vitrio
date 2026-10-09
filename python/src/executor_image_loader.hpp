// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `ExecutorImageLoader` to a module.
 *
 * @param m The module.
 */
void bind_executor_image_loader(nanobind::module_ &m);

} // namespace vitrio
