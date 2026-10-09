// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `ExecutorImageSaver` to a module.
 *
 * @param m The module.
 */
void bind_executor_image_saver(nanobind::module_ &m);

} // namespace vitrio
