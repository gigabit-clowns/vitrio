// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace vitrio
{

/**
 * @brief What a mapping of a file may be used for.
 */
enum class image_file_access
{
	/// The bytes of the file may only be read.
	read_only,

	/// The bytes of the file may be read and written.
	read_write,
};

} // namespace vitrio
