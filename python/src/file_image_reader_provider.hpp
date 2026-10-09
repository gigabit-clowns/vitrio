// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `FileImageReaderProvider` to a module.
 *
 * @param m The module.
 */
void bind_file_image_reader_provider(nanobind::module_ &m);

} // namespace vitrio
