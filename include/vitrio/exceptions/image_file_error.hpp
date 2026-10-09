// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <stdexcept>

#include <vitrio/export.hpp>
#include <vitrio/platform.hpp>

namespace vitrio
{

/**
 * @brief Exception indicating that an image file can not be reached.
 *
 * Thrown when the file itself is the obstacle, whatever it holds: it is
 * missing or unreadable, or it can not be created, sized, mapped or flushed.
 * A file that is reached but whose contents contradict its format is an
 * @ref image_file_format_error instead.
 */
VITRIO_STD_BASE_INTERFACE
class VITRIO_API image_file_error : public std::runtime_error
{
	using runtime_error::runtime_error;
};

} // namespace vitrio
