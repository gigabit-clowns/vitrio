// SPDX-License-Identifier: LGPL-2.1-or-later

#include "iovec_gathering.hpp"

#include <assert.hpp>

#include <algorithm>

namespace vitrio
{

std::size_t gather_iovecs(
	span<const memory_range> buffers,
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
			vectors[count].iov_base =
				static_cast<char*>(buffer.get_address()) + passed;
			vectors[count].iov_len = size;
			++count;
			bytes -= size;
		}
	}

	return count;
}

} // namespace vitrio
