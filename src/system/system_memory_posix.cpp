// SPDX-License-Identifier: LGPL-2.1-or-later

#include "system_memory.hpp"

#include <assert.hpp>

#include <sys/mman.h>

#include <new>

namespace vitrio
{

std::shared_ptr<byte> allocate_system_memory(std::size_t size)
{
	VITRIO_ASSERT(size > 0);

	auto *memory = ::mmap(
		nullptr,
		size,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON,
		-1,
		0
	);
	if (memory == MAP_FAILED)
	{
		throw std::bad_alloc();
	}

	return std::shared_ptr<byte>(
		static_cast<byte*>(memory),
		[size](byte *released) noexcept
		{
			::munmap(released, size);
		}
	);
}

} // namespace vitrio
