// SPDX-License-Identifier: LGPL-2.1-or-later

#include "page_prefetch.hpp"

#include <fcntl.h>

#include <algorithm>
#include <climits>
#include <cstdint>

namespace vitrio
{

void prefetch_pages(
	file_handle_t file,
	span<const byte> mapping,
	span<const memory_range> ranges
) noexcept
{
	// macOS states its advice for a file rather than for memory:
	// F_RDADVISE reads a stretch of the file into the cache, in the
	// background and without copying it out. A stretch of the mapping is the
	// same stretch of the file, since the mapping holds it from its first
	// byte. The count is an int, so a longer stretch is asked for in turns.
	const auto first = reinterpret_cast<std::uintptr_t>(mapping.data());
	for (const auto &range : ranges)
	{
		auto offset = static_cast<off_t>(
			reinterpret_cast<std::uintptr_t>(range.get_address()) - first
		);
		auto left = range.get_size();
		while (left > 0)
		{
			const auto count = std::min<std::size_t>(left, INT_MAX);
			radvisory advice;
			advice.ra_offset = offset;
			advice.ra_count = static_cast<int>(count);
			::fcntl(file, F_RDADVISE, &advice);
			offset += static_cast<off_t>(count);
			left -= count;
		}
	}
}

} // namespace vitrio
