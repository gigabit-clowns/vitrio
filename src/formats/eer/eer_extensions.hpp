// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

namespace vitrio
{
namespace eer
{

/**
 * @brief Check whether an extension is that of an EER file.
 *
 * @param extension The extension, folded to lower case and including its
 * leading dot, as @ref image_file_probe reports it.
 * @return bool true for ".eer".
 */
bool is_extension(const std::string &extension) noexcept;

} // namespace eer
} // namespace vitrio
