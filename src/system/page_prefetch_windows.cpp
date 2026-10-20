// SPDX-License-Identifier: LGPL-2.1-or-later

#include "page_prefetch.hpp"

#include "page_size.hpp"

#include <windows.h>

#include <array>
#include <cstdint>

namespace vitrio
{

namespace
{

// The layout of WIN32_MEMORY_RANGE_ENTRY, stated here rather than taken from
// the headers, which declare it only for Windows 8 and newer.
struct memory_range_entry
{
	void *address;
	SIZE_T size;
};

using prefetch_virtual_memory_function = BOOL (WINAPI *)(
	HANDLE,
	ULONG_PTR,
	memory_range_entry*,
	ULONG
);

// PrefetchVirtualMemory arrived in Windows 8, so it is resolved at run time:
// where it is missing the mapping is read as it always was, one fault at a
// time. kernel32 is loaded into every process, so the handle is never taken.
prefetch_virtual_memory_function get_prefetch_virtual_memory() noexcept
{
	static const auto function =
		reinterpret_cast<prefetch_virtual_memory_function>(
			reinterpret_cast<void*>(
				::GetProcAddress(
					::GetModuleHandleW(L"kernel32.dll"),
					"PrefetchVirtualMemory"
				)
			)
		);

	return function;
}

} // anonymous namespace

void prefetch_pages(span<const memory_range> ranges) noexcept
{
	const auto prefetch = get_prefetch_virtual_memory();
	if (prefetch == nullptr)
	{
		return;
	}

	const auto page_size = get_page_size();

	// Gathered a fixed batch at a time rather than into one allocation, so
	// that advising cannot throw where it is only a hint.
	constexpr std::size_t batch_size = 64;
	std::array<memory_range_entry, batch_size> entries;
	std::size_t count = 0;

	for (const auto &range : ranges)
	{
		// Widened to the page it starts in, as the POSIX call needs, so that
		// a stretch is advised alike everywhere.
		const auto first =
			reinterpret_cast<std::uintptr_t>(range.get_address());
		const auto lead = first % page_size;
		entries[count].address = reinterpret_cast<void*>(first - lead);
		entries[count].size = static_cast<SIZE_T>(range.get_size() + lead);
		++count;

		if (count == batch_size)
		{
			prefetch(::GetCurrentProcess(), count, entries.data(), 0);
			count = 0;
		}
	}

	if (count > 0)
	{
		prefetch(::GetCurrentProcess(), count, entries.data(), 0);
	}
}

} // namespace vitrio
