// SPDX-License-Identifier: LGPL-2.1-or-later

#include "array_implementation.hpp"

#include "array_memory.hpp"

#include <utility>

namespace vitrio
{

array_implementation::array_implementation(
	span<const byte> memory,
	std::shared_ptr<const void> owner,
	array_descriptor descriptor
)
	: m_memory(memory)
	, m_owner(std::move(owner))
	, m_descriptor(std::move(descriptor))
{
	check_array_memory(m_memory, m_descriptor);
}

array_implementation::~array_implementation() = default;

const array_descriptor& array_implementation::get_descriptor() const noexcept
{
	return m_descriptor;
}

const byte* array_implementation::get_data() const noexcept
{
	return m_memory.data();
}

const array_descriptor& array_implementation::get_empty_descriptor() noexcept
{
	static const array_descriptor descriptor;
	return descriptor;
}

} // namespace vitrio
