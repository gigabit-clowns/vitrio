// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/array/array_ref.hpp>

#include "array_implementation.hpp"
#include "array_memory.hpp"

#include <vitrio/array/array.hpp>

namespace vitrio
{

array_ref::array_ref() noexcept
	: m_data(nullptr)
	, m_descriptor(&array_implementation::get_empty_descriptor())
{
}

array_ref::array_ref(span<byte> memory, const array_descriptor &descriptor)
	: m_data(memory.data())
	, m_descriptor(&descriptor)
{
	check_array_memory(memory, descriptor);
}

array_ref::array_ref(array &other) noexcept
	: m_data(other.get_data())
	, m_descriptor(&other.get_descriptor())
{
}

const array_descriptor& array_ref::get_descriptor() const noexcept
{
	return *m_descriptor;
}

byte* array_ref::get_data() const noexcept
{
	return m_data;
}

} // namespace vitrio
