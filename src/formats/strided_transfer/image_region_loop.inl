// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_region_loop.hpp"

#include <cstddef>

namespace vitrio
{

template <typename Kernel, typename Destination, typename Source>
inline void run_region_inner_loop(
	const Kernel &kernel,
	const image_region_layout::axis &axis,
	Destination destination,
	Source source
)
{
	const auto extent = axis.get_extent();
	const auto destination_stride = axis.get_destination_stride();
	const auto source_stride = axis.get_source_stride();

	// The contiguous case is spelled out with an index, so that the compiler
	// sees a loop over two plain arrays.
	if (destination_stride == 1 && source_stride == 1)
	{
		for (std::size_t i = 0; i < extent; ++i)
		{
			kernel(destination + i, source + i);
		}
	}
	else
	{
		for (std::size_t i = 0; i < extent; ++i)
		{
			kernel(destination, source);
			destination += destination_stride;
			source += source_stride;
		}
	}
}

// Walks one axis and, through itself, the axes inside it. The depth of the
// recursion is the number of axes, so nothing is allocated to keep count.
template <typename Kernel, typename Destination, typename Source>
inline void run_region_axis(
	const Kernel &kernel,
	const image_region_layout &layout,
	std::size_t index,
	Destination destination,
	Source source
)
{
	const auto &axis = layout.get_axis(index);
	if (index == 0)
	{
		run_region_inner_loop(kernel, axis, destination, source);
		return;
	}

	const auto extent = axis.get_extent();
	for (std::size_t i = 0; i < extent; ++i)
	{
		run_region_axis(kernel, layout, index - 1, destination, source);
		destination += axis.get_destination_stride();
		source += axis.get_source_stride();
	}
}

template <typename Kernel, typename Destination, typename Source>
inline void run_region_loop(
	const Kernel &kernel,
	const image_region_layout &layout,
	Destination destination,
	Source source
)
{
	const auto axis_count = layout.get_axis_count();
	if (axis_count == 0)
	{
		// A region with no axes is a single element.
		kernel(destination, source);
		return;
	}

	run_region_axis(kernel, layout, axis_count - 1, destination, source);
}

} // namespace vitrio
