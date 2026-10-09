// SPDX-License-Identifier: LGPL-2.1-or-later

#include "page_prefetch.hpp"

#include <sys/mman.h>

namespace vitrio
{

namespace
{

// POSIX states posix_madvise, which is also the spelling that takes a void*
// everywhere; madvise is the older one, and is what a platform reaching only
// that offers. Where neither is there, a read faults its pages in as it goes.
#if defined(POSIX_MADV_WILLNEED)

	void advise_will_need(void *address, std::size_t size) noexcept
	{
		::posix_madvise(address, size, POSIX_MADV_WILLNEED);
	}

#elif defined(MADV_WILLNEED)

	void advise_will_need(void *address, std::size_t size) noexcept
	{
		::madvise(address, size, MADV_WILLNEED);
	}

#else

	#warning "No MADV_WILLNEED variant on this platform"
	void advise_will_need(void *, std::size_t) noexcept
	{
	}

#endif

} // anonymous namespace

void prefetch_pages(span<const memory_range> ranges) noexcept
{
	for (const auto &range : ranges)
	{
		advise_will_need(range.get_address(), range.get_size());
	}
}

} // namespace vitrio
