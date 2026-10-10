// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_region_layout.hpp"

#include <cstddef>
#include <type_traits>

namespace vitrio
{

/**
 * @brief The stride of a side that is contiguous along the innermost axis.
 *
 * It stands in for a @c std::ptrdiff_t of one, known when the code is
 * compiled. Every other stride is handed over as a @c std::ptrdiff_t.
 */
using region_unit_stride = std::integral_constant<std::ptrdiff_t, 1>;

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
	const image_region_layout &layout,
	Function &&function
);

/**
 * @brief Apply a kernel to every pair of elements of one region.
 *
 * The region is walked as its layout states, the destination first. Along
 * the innermost axis the kernel is called in a loop of its own, which is
 * what a compiler vectorizes when both sides are contiguous there.
 *
 * The strides of that axis are given apart from the layout, as
 * @ref dispatch_region_inner_strides hands them over, so that the loop is
 * compiled for them.
 *
 * @tparam Kernel Function called as @c kernel(destination,source), with a
 * pointer to one element of each side.
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
	const image_region_layout &layout,
	DestinationStride destination_stride,
	SourceStride source_stride,
	Destination destination,
	Source source
);

} // namespace vitrio

#include "image_region_loop.inl"
