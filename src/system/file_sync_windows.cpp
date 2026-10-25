// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_sync.hpp"

#include <windows.h>

namespace vitrio
{

void sync_file(file_handle_t file, std::error_code &error) noexcept
{
	error.clear();

	if (!::FlushFileBuffers(file))
	{
		error.assign(
			static_cast<int>(::GetLastError()),
			std::system_category()
		);
	}
}

} // namespace vitrio
