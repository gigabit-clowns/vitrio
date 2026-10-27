// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nanobind/nanobind.h>

namespace vitrio
{

/**
 * @brief Add `FileTransferPool` to a module.
 *
 * @param m The module.
 */
void bind_file_transfer_pool(nanobind::module_ &m);

} // namespace vitrio
