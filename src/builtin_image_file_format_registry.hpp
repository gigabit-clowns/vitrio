// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_file_read_format_registry.hpp>
#include <vitrio/image_file_write_format_registry.hpp>

namespace vitrio
{

/**
 * @brief Get the registry of the image read formats bundled with the library.
 *
 * Private to the library, so only the formats bundled with it can register
 * into it.
 *
 * @return image_file_read_format_registry& The registry.
 */
image_file_read_format_registry&
get_builtin_image_file_read_format_registry() noexcept;

/**
 * @brief Get the registry of the image write formats bundled with the
 * library.
 *
 * @return image_file_write_format_registry& The registry.
 *
 * @see get_builtin_image_file_read_format_registry
 */
image_file_write_format_registry&
get_builtin_image_file_write_format_registry() noexcept;

} // namespace vitrio
