// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `ImageWriterProvider` to a module.
 *
 * @param m The module.
 */
void bind_image_writer_provider(nanobind::module_ &m);

} // namespace vitrio
