// SPDX-License-Identifier: LGPL-2.1-or-later

// This file is a body, as image_region_loop_impl.hpp, which includes it, is.

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

// Walks the innermost axis element by element where a side is strided.
// Spelled out with an index, so that where the other side is contiguous its
// stride of region_unit_stride multiplies away when the code is compiled, and
// the compiler sees a loop over one plain array.
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
	const auto count = static_cast<std::ptrdiff_t>(extent);
	for (std::ptrdiff_t i = 0; i < count; ++i)
	{
		kernel(
			destination + i * destination_stride,
			source + i * source_stride
		);
	}
}

// Where both sides are contiguous the whole axis is one run, and the kernel
// is handed it as such. This overload is the more specialized of the two, so
// it is the one chosen for a pair of unit strides.
template <typename Kernel, typename Destination, typename Source>
inline void run_region_inner_loop(
	const Kernel &kernel,
	std::size_t extent,
	region_unit_stride,
	region_unit_stride,
	Destination destination,
	Source source
)
{
	kernel(destination, source, extent);
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

template <
	typename Kernel,
	typename DestinationPointer,
	typename SourcePointer
>
inline void run_regions(
	const Kernel &kernel,
	const image_region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	DestinationPointer destination_data,
	std::ptrdiff_t destination_origin,
	SourcePointer source_data,
	std::ptrdiff_t source_origin
)
{
	dispatch_region_inner_strides(
		layout,
		[&] (auto destination_stride, auto source_stride)
		{
			for (std::size_t i = 0; i < destination_offsets.size(); ++i)
			{
				run_region_loop(
					kernel,
					layout,
					destination_stride,
					source_stride,
					destination_data +
						(destination_offsets[i] - destination_origin),
					source_data + (source_offsets[i] - source_origin)
				);
			}
		}
	);
}
