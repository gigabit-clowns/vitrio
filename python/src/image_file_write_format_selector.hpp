// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `ImageFileWriteFormatSelector` to a module.
 *
 * @param m The module.
 */
void bind_image_file_write_format_selector(nanobind::module_ &m);

} // namespace vitrio
