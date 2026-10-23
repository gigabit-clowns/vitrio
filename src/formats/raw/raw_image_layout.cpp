// SPDX-License-Identifier: LGPL-2.1-or-later

#include "raw_image_layout.hpp"

#include <vitrio/array/numerical_type.hpp>

#include <functional>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace vitrio
{

raw_image_layout::raw_image_layout(
	image_descriptor descriptor,
	std::vector<std::ptrdiff_t> strides,
	std::size_t data_offset,
	byte_order order
)
	: m_descriptor(std::move(descriptor))
	, m_strides(std::move(strides))
	, m_data_offset(data_offset)
	, m_byte_order(order)
{
	if (m_strides.size() != m_descriptor.get_extents().size())
	{
		throw std::invalid_argument(
			"raw_image_layout: The strides do not have the rank of the "
			"extents."
		);
	}
}

const image_descriptor& raw_image_layout::get_descriptor() const noexcept
{
	return m_descriptor;
}

span<const std::ptrdiff_t> raw_image_layout::get_strides() const noexcept
{
	return make_span(m_strides.data(), m_strides.size());
}

std::size_t raw_image_layout::get_data_offset() const noexcept
{
	return m_data_offset;
}

std::size_t raw_image_layout::get_data_size() const noexcept
{
	const auto extents = m_descriptor.get_extents();
	const auto element_count = std::accumulate(
		extents.begin(),
		extents.end(),
		std::size_t(1),
		std::multiplies<std::size_t>()
	);

	return element_count * get_size(m_descriptor.get_data_type());
}

byte_order raw_image_layout::get_byte_order() const noexcept
{
	return m_byte_order;
}

bool raw_image_layout::is_aligned() const noexcept
{
	return m_data_offset % get_size(m_descriptor.get_data_type()) == 0;
}

bool raw_image_layout::is_contiguous() const noexcept
{
	const auto extents = m_descriptor.get_extents();
	std::ptrdiff_t stride = 1;
	for (auto axis = extents.size(); axis > 0; --axis)
	{
		if (m_strides[axis - 1] != stride)
		{
			return false;
		}
		stride *= static_cast<std::ptrdiff_t>(extents[axis - 1]);
	}

	return true;
}

} // namespace vitrio
