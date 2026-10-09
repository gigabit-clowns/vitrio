// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <stdexcept>

#include <vitrio/export.hpp>
#include <vitrio/platform.hpp>

namespace vitrio
{

/**
 * @brief Exception indicating that an image file can not be interpreted.
 *
 * Thrown when the contents of a file contradict the format that is reading
 * it: a malformed or truncated header, a declared size that the file can not
 * hold, or an encoding that the format recognizes but does not implement. A
 * file that can not be reached at all is an @ref image_file_error instead.
 */
VITRIO_STD_BASE_INTERFACE
class VITRIO_API image_file_format_error : public std::runtime_error
{
	using runtime_error::runtime_error;
};

} // namespace vitrio
