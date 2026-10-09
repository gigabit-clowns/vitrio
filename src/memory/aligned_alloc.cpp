// SPDX-License-Identifier: LGPL-2.1-or-later

#include "aligned_alloc.hpp"

#include <algorithm>
#include <cstdlib>

#if defined(_WIN32)
	#include <malloc.h>
#endif

namespace vitrio
{

void* aligned_alloc(std::size_t size, std::size_t alignment) noexcept
{
	#if defined(_WIN32)
		return _aligned_malloc(size, alignment);
	#else
		// posix_memalign refuses an alignment below that of a pointer.
		void *result = nullptr;
		alignment = std::max(alignment, sizeof(void*));
		if (posix_memalign(&result, alignment, size) != 0)
		{
			result = nullptr;
		}
		return result;
	#endif
}

void aligned_free(void *ptr) noexcept
{
	#if defined(_WIN32)
		_aligned_free(ptr);
	#else
		std::free(ptr);
	#endif
}

} // namespace vitrio
