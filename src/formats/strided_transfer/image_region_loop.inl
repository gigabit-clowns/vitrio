// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_region_loop.hpp"

#include <cstddef>

namespace vitrio
{

template <typename Function>
inline void dispatch_region_inner_strides(
	const image_region_layout &layout,
	Function &&function
)
{
	if (layout.get_axis_count() == 0)
	{
		function(region_unit_stride(), region_unit_stride());
		return;
	}

	const auto &axis = layout.get_axis(0);
	const auto destination_stride = axis.get_destination_stride();
	const auto source_stride = axis.get_source_stride();

	if (destination_stride == 1 && source_stride == 1)
	{
		function(region_unit_stride(), region_unit_stride());
	}
	else if (destination_stride == 1)
	{
		function(region_unit_stride(), source_stride);
	}
	else if (source_stride == 1)
	{
		function(destination_stride, region_unit_stride());
	}
	else
	{
		function(destination_stride, source_stride);
	}
}

template <
	typename Kernel,
	typename DestinationStride,
	typename SourceStride,
	typename Destination,
	typename Source
>
inline void run_region_inner_loop(
	const Kernel &kernel,
	std::size_t extent,
	DestinationStride destination_stride,
	SourceStride source_stride,
	Destination destination,
	Source source
)
{
	// Spelled out with an index, so that where both sides are contiguous the
	// compiler sees a loop over two plain arrays: a stride of
	// region_unit_stride multiplies away when the code is compiled.
	const auto count = static_cast<std::ptrdiff_t>(extent);
	for (std::ptrdiff_t i = 0; i < count; ++i)
	{
		kernel(
			destination + i * destination_stride,
			source + i * source_stride
		);
	}
}

// Walks one axis and, through itself, the axes inside it. The depth of the
// recursion is the number of axes, so nothing is allocated to keep count.
template <
	typename Kernel,
	typename DestinationStride,
	typename SourceStride,
	typename Destination,
	typename Source
>
inline void run_region_axis(
	const Kernel &kernel,
	const image_region_layout &layout,
	std::size_t index,
	DestinationStride inner_destination_stride,
	SourceStride inner_source_stride,
	Destination destination,
	Source source
)
{
	const auto &axis = layout.get_axis(index);
	if (index == 0)
	{
		run_region_inner_loop(
			kernel,
			axis.get_extent(),
			inner_destination_stride,
			inner_source_stride,
			destination,
			source
		);
		return;
	}

	const auto extent = axis.get_extent();
	for (std::size_t i = 0; i < extent; ++i)
	{
		run_region_axis(
			kernel,
			layout,
			index - 1,
			inner_destination_stride,
			inner_source_stride,
			destination,
			source
		);
		destination += axis.get_destination_stride();
		source += axis.get_source_stride();
	}
}

template <
	typename Kernel,
	typename DestinationStride,
	typename SourceStride,
	typename Destination,
	typename Source
>
inline void run_region_loop(
	const Kernel &kernel,
	const image_region_layout &layout,
	DestinationStride destination_stride,
	SourceStride source_stride,
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

	run_region_axis(
		kernel,
		layout,
		axis_count - 1,
		destination_stride,
		source_stride,
		destination,
		source
	);
}

} // namespace vitrio
