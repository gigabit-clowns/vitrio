// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `ScratchImageReaderProvider` to a module.
 *
 * @param m The module.
 */
void bind_scratch_image_reader_provider(nanobind::module_ &m);

} // namespace vitrio
