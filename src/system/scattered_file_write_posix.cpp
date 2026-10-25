// SPDX-License-Identifier: LGPL-2.1-or-later

#include "scattered_file_write.hpp"

#include "iovec_gathering.hpp"

#include <sys/types.h>
#include <sys/uio.h>

#include <array>
#include <cerrno>

namespace vitrio
{

namespace
{

// Linux and macOS take 1024 buffers in one call. Fewer are handed over at
// once, so that they fit on the stack.
constexpr std::size_t buffers_per_call = 64;

// macOS refuses to write more than INT_MAX bytes in one call, and Linux
// writes no more than about 2 GiB.
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

	std::array<iovec, buffers_per_call> vectors;
	std::size_t total = 0;
	for (;;)
	{
		const auto count = gather_iovecs(
			buffers,
			total,
			make_span(vectors),
			bytes_per_call
		);
		if (count == 0)
		{
			return total;
		}

		ssize_t result;
		do
		{
			result = ::pwritev(
				file,
				vectors.data(),
				static_cast<int>(count),
				static_cast<off_t>(offset + total)
			);
		}
		while (result < 0 && errno == EINTR);

		if (result < 0)
		{
			error.assign(errno, std::system_category());
			return 0;
		}

		// A write that moves nothing would move nothing again: the system
		// gave no reason, and none is made up.
		if (result == 0)
		{
			error = std::make_error_code(std::errc::io_error);
			return 0;
		}

		total += static_cast<std::size_t>(result);
	}
}

} // namespace vitrio
