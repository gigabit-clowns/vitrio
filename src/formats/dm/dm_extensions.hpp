// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

namespace vitrio
{
namespace dm
{

/**
 * @brief Check whether an extension is one a DigitalMicrograph file of
 * version 3 or 4 is read from.
 *
 * @param extension The extension, folded to lower case and including its
 * leading dot, as @ref image_file_probe reports it.
 * @return bool true for `.dm3` and `.dm4`.
 */
bool is_readable_extension(const std::string &extension) noexcept;

} // namespace dm
} // namespace vitrio
