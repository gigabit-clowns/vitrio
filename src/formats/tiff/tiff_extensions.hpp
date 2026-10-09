// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

namespace vitrio
{
namespace tiff
{

/**
 * @brief Check whether an extension is one a TIFF file is created with.
 *
 * @param extension The extension, folded to lower case and including its
 * leading dot, as @ref image_probe reports it.
 * @return bool true if a TIFF file is created with it.
 */
bool is_writable_extension(const std::string &extension) noexcept;

} // namespace tiff
} // namespace vitrio
