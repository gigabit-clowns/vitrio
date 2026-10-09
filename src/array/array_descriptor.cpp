// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/array/array_descriptor.hpp>

#include "checked_arithmetic.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace vitrio
{

array_descriptor::array_descriptor() noexcept
	: m_offset(0)
	, m_data_type(numerical_type::unknown)
{
}

array_descriptor::array_descriptor(
	std::vector<std::size_t> extents,
	std::vector<std::ptrdiff_t> strides,
	std::ptrdiff_t offset,
	numerical_type data_type
)
	: m_extents(std::move(extents))
	, m_strides(std::move(strides))
	, m_offset(offset)
	, m_data_type(data_type)
{
	if (m_strides.size() != m_extents.size())
	{
		throw std::invalid_argument(
			"array_descriptor: There must be one stride per extent."
		);
	}
	if (get_size(m_data_type) == 0)
	{
		throw std::invalid_argument(
			"array_descriptor: The data type must not be unknown."
		);
	}
}

array_descriptor::array_descriptor(const array_descriptor &other) = default;

array_descriptor::array_descriptor(array_descriptor &&other) noexcept =
	default;

array_descriptor::~array_descriptor() = default;

array_descriptor&
array_descriptor::operator=(const array_descriptor &other) = default;

array_descriptor&
array_descriptor::operator=(array_descriptor &&other) noexcept = default;

span<const std::size_t> array_descriptor::get_extents() const noexcept
{
	return make_span(m_extents);
}

span<const std::ptrdiff_t> array_descriptor::get_strides() const noexcept
{
	return make_span(m_strides);
}

std::ptrdiff_t array_descriptor::get_offset() const noexcept
{
	return m_offset;
}

numerical_type array_descriptor::get_data_type() const noexcept
{
	return m_data_type;
}

array_descriptor make_contiguous_array_descriptor(
	span<const std::size_t> extents,
	numerical_type data_type
)
{
	const auto highest = std::numeric_limits<std::ptrdiff_t>::max();

	// The stride of an axis is how many elements the axes after it hold.
	std::vector<std::ptrdiff_t> strides(extents.size());
	std::ptrdiff_t stride = 1;
	for (auto axis = extents.size(); axis > 0; --axis)
	{
		strides[axis - 1] = stride;

		const auto extent = extents[axis - 1];
		const auto fits =
			extent <= static_cast<std::size_t>(highest) &&
			checked_multiply(
				stride,
				static_cast<std::ptrdiff_t>(extent),
				stride
			);
		if (!fits)
		{
			throw std::out_of_range(
				"make_contiguous_array_descriptor: The extents hold more "
				"elements than can be addressed."
			);
		}
	}

	return array_descriptor(
		std::vector<std::size_t>(extents.begin(), extents.end()),
		std::move(strides),
		0,
		data_type
	);
}

bool is_initialized(const array_descriptor &descriptor) noexcept
{
	return descriptor.get_data_type() != numerical_type::unknown;
}

} // namespace vitrio
