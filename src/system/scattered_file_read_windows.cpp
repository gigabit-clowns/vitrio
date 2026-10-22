// SPDX-License-Identifier: LGPL-2.1-or-later

#include "scattered_file_read.hpp"

#include <windows.h>

#include <algorithm>

namespace vitrio
{

namespace
{

// ReadFile takes the size of a read as 32 bits.
constexpr std::size_t bytes_per_call = std::size_t{1} << 30;

} // anonymous namespace

std::size_t read_scattered(
	file_handle_t file,
	std::uint64_t offset,
	span<const memory_range> buffers,
	std::error_code &error
) noexcept
{
	error.clear();

	// Windows scatters a read only into whole pages of a file opened without
	// its cache, so every buffer is read on its own.
	std::size_t total = 0;
	for (const auto &buffer : buffers)
	{
		auto *address = static_cast<char*>(buffer.get_address());
		auto left = buffer.get_size();
		while (left > 0)
		{
			const auto position = offset + total;
			OVERLAPPED overlapped = {};
			overlapped.Offset = static_cast<DWORD>(position & 0xFFFFFFFFU);
			overlapped.OffsetHigh = static_cast<DWORD>(position >> 32);

			DWORD read = 0;
			if (!::ReadFile(
				file,
				address,
				static_cast<DWORD>(std::min(left, bytes_per_call)),
				&read,
				&overlapped
			))
			{
				const auto failure = ::GetLastError();
				if (failure == ERROR_HANDLE_EOF)
				{
					return total;
				}

				error.assign(static_cast<int>(failure), std::system_category());
				return 0;
			}

			if (read == 0)
			{
				return total;
			}

			total += read;
			address += read;
			left -= read;
		}
	}

	return total;
}

} // namespace vitrio
