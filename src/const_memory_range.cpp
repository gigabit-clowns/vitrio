// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/const_memory_range.hpp>

#include <vitrio/memory_range.hpp>

namespace vitrio
{

const_memory_range::const_memory_range(
	const void *address,
	std::size_t size
) noexcept
	: m_address(address)
	, m_size(size)
{
}

const_memory_range::const_memory_range(const memory_range &range) noexcept
	: m_address(range.get_address())
	, m_size(range.get_size())
{
}

const void* const_memory_range::get_address() const noexcept
{
	return m_address;
}

std::size_t const_memory_range::get_size() const noexcept
{
	return m_size;
}

} // namespace vitrio
