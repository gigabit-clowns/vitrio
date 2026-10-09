// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/array/const_array.hpp>

#include "array_implementation.hpp"

#include <stdexcept>
#include <utility>

namespace vitrio
{

const_array::const_array() noexcept = default;

const_array::const_array(
	span<const byte> memory,
	std::shared_ptr<const void> owner,
	array_descriptor descriptor
)
{
	if (!owner)
	{
		throw std::invalid_argument(
			"const_array: The owner must not be null."
		);
	}

	m_implementation = std::make_shared<array_implementation>(
		memory,
		std::move(owner),
		std::move(descriptor)
	);
}

const_array::const_array(
	std::shared_ptr<const array_implementation> implementation
) noexcept
	: m_implementation(std::move(implementation))
{
}

const_array::const_array(const_array &&other) noexcept = default;

const_array::~const_array() = default;

const_array& const_array::operator=(const_array &&other) noexcept = default;

const array_descriptor& const_array::get_descriptor() const noexcept
{
	return m_implementation
		? m_implementation->get_descriptor()
		: array_implementation::get_empty_descriptor();
}

const byte* const_array::get_data() const noexcept
{
	return m_implementation ? m_implementation->get_data() : nullptr;
}

const_array const_array::share() const noexcept
{
	return const_array(m_implementation);
}

} // namespace vitrio
