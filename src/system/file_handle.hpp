// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace vitrio
{

/**
 * @brief A file as the operating system names it to a process: a descriptor
 * where files have them, and a handle on Windows.
 */
#if defined(_WIN32)
	using file_handle_t = void*;
#else
	using file_handle_t = int;
#endif

} // namespace vitrio
