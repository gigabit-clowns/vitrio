// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_handle.hpp"

#include <system_error>

namespace vitrio
{

/**
 * @brief Make what was written to a file reach the storage.
 *
 * @param file The file, open for writing.
 * @param error Set to what the system failed the sync with, and cleared
 * where it succeeded.
 */
void sync_file(file_handle_t file, std::error_code &error) noexcept;

} // namespace vitrio
