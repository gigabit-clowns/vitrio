// SPDX-License-Identifier: LGPL-2.1-or-later

#include "memory_range.hpp"

namespace vitrio
{

memory_range::memory_range(void *address, std::size_t size) noexcept
	: m_address(address)
	, m_size(size)
{
}

void* memory_range::get_address() const noexcept
{
	return m_address;
}

std::size_t memory_range::get_size() const noexcept
{
	return m_size;
}

} // namespace vitrio
