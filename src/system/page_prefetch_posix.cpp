// SPDX-License-Identifier: LGPL-2.1-or-later

#include "page_prefetch.hpp"

#include "page_size.hpp"

#include <sys/mman.h>

#include <cstdint>

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

void prefetch_pages(
	file_handle_t,
	span<const byte>,
	span<const memory_range> ranges
) noexcept
{
	const auto page_size = get_page_size();
	for (const auto &range : ranges)
	{
		// The call takes a stretch that starts on a page.
		const auto first =
			reinterpret_cast<std::uintptr_t>(range.get_address());
		const auto lead = first % page_size;
		advise_will_need(
			reinterpret_cast<void*>(first - lead),
			range.get_size() + lead
		);
	}
}

} // namespace vitrio
