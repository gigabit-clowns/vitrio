// SPDX-License-Identifier: LGPL-2.1-or-later

#include "system_memory.hpp"

#include <assert.hpp>

#include <windows.h>

#include <new>

namespace vitrio
{

std::shared_ptr<byte> allocate_system_memory(std::size_t size)
{
	VITRIO_ASSERT(size > 0);

	auto *memory = ::VirtualAlloc(
		nullptr,
		size,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_READWRITE
	);
	if (memory == nullptr)
	{
		throw std::bad_alloc();
	}

	return std::shared_ptr<byte>(
		static_cast<byte*>(memory),
		[](byte *released) noexcept
		{
			::VirtualFree(released, 0, MEM_RELEASE);
		}
	);
}

} // namespace vitrio
