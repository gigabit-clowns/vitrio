// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_sync.hpp"

#include <unistd.h>

#include <cerrno>

namespace vitrio
{

void sync_file(file_handle_t file, std::error_code &error) noexcept
{
	error.clear();

	int result;
	do
	{
		result = ::fsync(file);
	}
	while (result < 0 && errno == EINTR);

	if (result < 0)
	{
		error.assign(errno, std::system_category());
	}
}

} // namespace vitrio
