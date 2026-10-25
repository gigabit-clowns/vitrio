// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_chunk_layout.hpp"

#include <array/checked_arithmetic.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace vitrio
{
namespace zarr
{

namespace
{

[[noreturn]]
void overflow()
{
	throw std::overflow_error(
		"zarr_chunk_layout: The bytes of a chunk can not be counted."
	);
}

std::ptrdiff_t to_signed(std::size_t value)
{
	if (value > static_cast<std::size_t>(
		std::numeric_limits<std::ptrdiff_t>::max()))
	{
		overflow();
	}

	return static_cast<std::ptrdiff_t>(value);
}

} // anonymous namespace

zarr_chunk_layout::zarr_chunk_layout(
	std::vector<std::size_t> extents,
	span<const std::size_t> memory_order,
	numerical_type type,
	byte_order order
)
	: m_extents(std::move(extents))
	, m_strides(m_extents.size(), 0)
	, m_type(type)
	, m_order(order)
	, m_element_count(1)
{
	const auto rank = m_extents.size();
	std::vector<bool> seen(rank, false);
	if (memory_order.size() != rank)
	{
		throw std::invalid_argument(
			"zarr_chunk_layout: The order of the axes in memory does not "
			"name every axis."
		);
	}
	for (const auto axis : memory_order)
	{
		if (axis >= rank || seen[axis])
		{
			throw std::invalid_argument(
				"zarr_chunk_layout: The order of the axes in memory is not a "
				"permutation of them."
			);
		}
		seen[axis] = true;
	}

	// C order over the axes as memory takes them: the last is contiguous.
	std::ptrdiff_t stride = 1;
	for (auto position = rank; position-- > 0;)
	{
		const auto axis = memory_order[position];
		m_strides[axis] = stride;
		if (!checked_multiply(stride, to_signed(m_extents[axis]), stride))
		{
			overflow();
		}
	}

	std::ptrdiff_t bytes;
	if (!checked_multiply(
		stride, static_cast<std::ptrdiff_t>(get_size(type)), bytes))
	{
		overflow();
	}
	m_element_count = static_cast<std::size_t>(stride);
}

span<const std::size_t> zarr_chunk_layout::get_extents() const noexcept
{
	return make_span(m_extents);
}

span<const std::ptrdiff_t> zarr_chunk_layout::get_strides() const noexcept
{
	return make_span(m_strides);
}

numerical_type zarr_chunk_layout::get_data_type() const noexcept
{
	return m_type;
}

byte_order zarr_chunk_layout::get_byte_order() const noexcept
{
	return m_order;
}

std::size_t zarr_chunk_layout::get_element_count() const noexcept
{
	return m_element_count;
}

std::size_t zarr_chunk_layout::get_byte_size() const noexcept
{
	return m_element_count * get_size(m_type);
}

} // namespace zarr
} // namespace vitrio
