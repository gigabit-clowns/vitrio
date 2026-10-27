// SPDX-License-Identifier: LGPL-2.1-or-later

#include "iovec_gathering.hpp"

#include <assert.hpp>

#include <algorithm>

namespace vitrio
{

namespace
{

template <typename Range>
std::size_t gather(
	span<const Range> buffers,
	std::size_t skipped,
	span<iovec> vectors,
	std::size_t bytes
) noexcept
{
	VITRIO_ASSERT(bytes > 0);

	std::size_t count = 0;
	for (const auto &buffer : buffers)
	{
		if (count == vectors.size() || bytes == 0)
		{
			break;
		}

		const auto passed = std::min(skipped, buffer.get_size());
		skipped -= passed;

		const auto size = std::min(buffer.get_size() - passed, bytes);
		if (size > 0)
		{
			// The system only reads through the vectors of a write.
			vectors[count].iov_base = const_cast<char*>(
				static_cast<const char*>(buffer.get_address()) + passed
			);
			vectors[count].iov_len = size;
			++count;
			bytes -= size;
		}
	}

	return count;
}

} // anonymous namespace

std::size_t gather_iovecs(
	span<const memory_range> buffers,
	std::size_t skipped,
	span<iovec> vectors,
	std::size_t bytes
) noexcept
{
	return gather(buffers, skipped, vectors, bytes);
}

std::size_t gather_iovecs(
	span<const const_memory_range> buffers,
	std::size_t skipped,
	span<iovec> vectors,
	std::size_t bytes
) noexcept
{
	return gather(buffers, skipped, vectors, bytes);
}

} // namespace vitrio
