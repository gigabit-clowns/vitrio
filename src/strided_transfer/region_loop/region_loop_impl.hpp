// SPDX-License-Identifier: LGPL-2.1-or-later

// This file is a body. It is included inside a namespace, by a header that
// has included what it needs first, and it has no guard, so that a
// translation unit can compile it into more than one namespace, each under
// compiler attributes of its own. It includes nothing of its own but its
// .inl, and opens no namespace. region_loop.hpp is the header that
// owns it.

/**
 * @brief Call a function with the strides of the innermost axis of a layout,
 * each as a @ref region_unit_stride when it is one and as a
 * @c std::ptrdiff_t otherwise.
 *
 * It is what chooses the inner loop of a transfer, once for all of its
 * regions: the function is instantiated for the four pairs of stride types,
 * and inside each the stride of a contiguous side is a constant.
 *
 * A layout with no axes walks a single element, which has no stride, and is
 * handed over as contiguous on both sides.
 *
 * @tparam Function Function called as
 * @c function(destination_stride,source_stride).
 * @param layout The space the regions are walked in.
 * @param function The function to call.
 */
template <typename Function>
void dispatch_region_inner_strides(
	const region_layout &layout,
	Function &&function
);

/**
 * @brief Apply a kernel to every pair of elements of one region.
 *
 * The region is walked as its layout states, the destination first. Along
 * the innermost axis the kernel is called element by element in a loop of
 * its own, which is what a compiler vectorizes, except where both sides are
 * contiguous there: then the kernel is handed the whole axis as one run,
 * and loops over it as it sees fit.
 *
 * The strides of that axis are given apart from the layout, as
 * @ref dispatch_region_inner_strides hands them over, so that the loop is
 * compiled for them.
 *
 * @tparam Kernel Function called as @c kernel(destination,source), with a
 * pointer to one element of each side, and as
 * @c kernel(destination,source,count) with pointers to the first elements
 * of a run of @c count that is contiguous on both sides.
 * @tparam DestinationStride @ref region_unit_stride or @c std::ptrdiff_t.
 * @tparam SourceStride @ref region_unit_stride or @c std::ptrdiff_t.
 * @tparam Destination Pointer to the elements of the destination.
 * @tparam Source Pointer to the elements of the source.
 * @param kernel The function to apply.
 * @param layout The space the region is walked in.
 * @param destination_stride Stride of the destination along the innermost
 * axis of @p layout.
 * @param source_stride Stride of the source along the innermost axis of
 * @p layout.
 * @param destination The first element of the region in the destination.
 * @param source The first element of the region in the source.
 */
template <
	typename Kernel,
	typename DestinationStride,
	typename SourceStride,
	typename Destination,
	typename Source
>
void run_region_loop(
	const Kernel &kernel,
	const region_layout &layout,
	DestinationStride destination_stride,
	SourceStride source_stride,
	Destination destination,
	Source source
);

/**
 * @brief Walk every region with one layout.
 *
 * Each region is the same space reached through a different pair of
 * pointers, so the layout is built once and only the two bases move. The
 * inner loop is chosen once too, before the first region, as the strides of
 * the innermost axis are the same for all of them.
 *
 * Either side may hold its elements from an origin on rather than from its
 * first, as a buffer holding a stretch of a file does, and its offsets are
 * then taken from there.
 *
 * The destination comes first, as it does in the layout.
 *
 * @tparam Kernel As in @ref run_region_loop.
 * @tparam DestinationPointer Pointer to the elements of the destination.
 * @tparam SourcePointer Pointer to the elements of the source.
 * @param kernel The function to apply.
 * @param layout The space every region is walked in.
 * @param destination_offsets Where each region starts in the destination,
 * in elements from its first.
 * @param source_offsets Where each region starts in the source, in elements
 * from its first.
 * @param destination_data The element of the destination at
 * @p destination_origin.
 * @param destination_origin The offset @p destination_data holds.
 * @param source_data The element of the source at @p source_origin.
 * @param source_origin The offset @p source_data holds.
 */
template <
	typename Kernel,
	typename DestinationPointer,
	typename SourcePointer
>
void run_regions(
	const Kernel &kernel,
	const region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	DestinationPointer destination_data,
	std::ptrdiff_t destination_origin,
	SourcePointer source_data,
	std::ptrdiff_t source_origin
);

/**
 * @brief Walk every region with one layout whose innermost axis is
 * contiguous on both sides.
 *
 * It is @ref run_regions for the layouts that hand the kernel runs, and
 * compiles the loop for two unit strides alone: a unit that compiles the
 * walk for a kernel whose run operation is the point of it is spared the
 * loops over strided elements, which it would walk no faster than any other.
 *
 * @pre @c layout.is_inner_axis_contiguous()
 * @see run_regions for the arguments.
 */
template <
	typename Kernel,
	typename DestinationPointer,
	typename SourcePointer
>
void run_contiguous_regions(
	const Kernel &kernel,
	const region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	DestinationPointer destination_data,
	std::ptrdiff_t destination_origin,
	SourcePointer source_data,
	std::ptrdiff_t source_origin
);

#include "region_loop_impl.inl"
