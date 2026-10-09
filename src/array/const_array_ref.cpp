// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/array/const_array_ref.hpp>

#include "array_implementation.hpp"
#include "array_memory.hpp"

#include <vitrio/array/array.hpp>
#include <vitrio/array/const_array.hpp>

namespace vitrio
{

const_array_ref::const_array_ref() noexcept
	: m_data(nullptr)
	, m_descriptor(&array_implementation::get_empty_descriptor())
{
}

const_array_ref::const_array_ref(
	span<const byte> memory,
	const array_descriptor &descriptor
)
	: m_data(memory.data())
	, m_descriptor(&descriptor)
{
	check_array_memory(memory, descriptor);
}

const_array_ref::const_array_ref(const array &other) noexcept
	: m_data(other.get_data())
	, m_descriptor(&other.get_descriptor())
{
}

const_array_ref::const_array_ref(const const_array &other) noexcept
	: m_data(other.get_data())
	, m_descriptor(&other.get_descriptor())
{
}

const_array_ref::const_array_ref(array_ref other) noexcept
	: m_data(other.get_data())
	, m_descriptor(&other.get_descriptor())
{
}

const array_descriptor& const_array_ref::get_descriptor() const noexcept
{
	return *m_descriptor;
}

const byte* const_array_ref::get_data() const noexcept
{
	return m_data;
}

} // namespace vitrio
