// SPDX-License-Identifier: LGPL-2.1-or-later

#include "scattered_file_write.hpp"

#include <windows.h>

#include <algorithm>

namespace vitrio
{

namespace
{

// WriteFile takes the size of a write as 32 bits.
constexpr std::size_t bytes_per_call = std::size_t{1} << 30;

} // anonymous namespace

std::size_t write_scattered(
	file_handle_t file,
	std::uint64_t offset,
	span<const const_memory_range> buffers,
	std::error_code &error
) noexcept
{
	error.clear();

	// Windows gathers a write only from whole pages of a file opened without
	// its cache, so every buffer is written on its own.
	std::size_t total = 0;
	for (const auto &buffer : buffers)
	{
		const auto *address = static_cast<const char*>(buffer.get_address());
		auto left = buffer.get_size();
		while (left > 0)
		{
			const auto position = offset + total;
			OVERLAPPED overlapped = {};
			overlapped.Offset = static_cast<DWORD>(position & 0xFFFFFFFFU);
			overlapped.OffsetHigh = static_cast<DWORD>(position >> 32);

			DWORD written = 0;
			if (!::WriteFile(
				file,
				address,
				static_cast<DWORD>(std::min(left, bytes_per_call)),
				&written,
				&overlapped
			))
			{
				error.assign(
					static_cast<int>(::GetLastError()),
					std::system_category()
				);
				return 0;
			}

			// A write that moves nothing would move nothing again.
			if (written == 0)
			{
				error = std::make_error_code(std::errc::io_error);
				return 0;
			}

			total += written;
			address += written;
			left -= written;
		}
	}

	return total;
}

} // namespace vitrio
