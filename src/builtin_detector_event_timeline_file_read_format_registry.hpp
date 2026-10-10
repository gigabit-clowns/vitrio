// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_timeline_file_read_format_registry.hpp>

namespace vitrio
{

/**
 * @brief Get the registry of the file formats of detector events bundled
 * with the library.
 *
 * Private to the library, so only the formats bundled with it can register
 * into it.
 *
 * @return detector_event_timeline_file_read_format_registry& The registry.
 *
 * @see get_builtin_image_file_read_format_registry
 */
detector_event_timeline_file_read_format_registry&
get_builtin_detector_event_timeline_file_read_format_registry() noexcept;

} // namespace vitrio
