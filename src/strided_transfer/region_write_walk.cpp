// SPDX-License-Identifier: LGPL-2.1-or-later

#include "region_write_walk.hpp"

namespace vitrio
{

region_write_walk::region_write_walk(
	const image_transfer_plan &regions,
	span<const std::size_t> file_extents,
	span<const std::ptrdiff_t> file_strides,
	span<const std::size_t> array_extents,
	span<const std::ptrdiff_t> array_strides,
	std::ptrdiff_t array_offset
)
	: m_offsets(
		regions,
		file_extents,
		file_strides,
		array_extents,
		array_strides,
		array_offset
	)
	, m_layout(regions, file_strides, array_strides)
{
}

const region_offsets&
region_write_walk::get_offsets() const noexcept
{
	return m_offsets;
}

const region_layout&
region_write_walk::get_layout() const noexcept
{
	return m_layout;
}

} // namespace vitrio
