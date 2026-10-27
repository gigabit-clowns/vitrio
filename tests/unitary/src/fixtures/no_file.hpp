// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <system/file_handle.hpp>

namespace vitrio
{

/**
 * @brief A handle that names no open file, which every read of it fails.
 */
#if defined(_WIN32)
	const file_handle_t no_file = nullptr;
#else
	const file_handle_t no_file = -1;
#endif

} // namespace vitrio
