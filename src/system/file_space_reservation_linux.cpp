// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_space_reservation.hpp"

#include <fcntl.h>

#include <cerrno>

namespace vitrio
{

void reserve_file_space(
	file_handle_t file,
	std::uint64_t size,
	std::error_code &error
) noexcept
{
	error.clear();
	if (size == 0)
	{
		return;
	}

	// fallocate itself, since posix_fallocate writes the whole file where
	// the file system can not reserve room, as ZFS can not.
	int result;
	do
	{
		result = ::fallocate(file, 0, 0, static_cast<off_t>(size));
	}
	while (result < 0 && errno == EINTR);

	if (result < 0 && errno != EOPNOTSUPP && errno != ENOSYS)
	{
		error.assign(errno, std::system_category());
	}
}

} // namespace vitrio
