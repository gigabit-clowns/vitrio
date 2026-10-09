// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_region_layout.hpp"

namespace vitrio
{

/**
 * @brief Apply a kernel to every pair of elements of one region.
 *
 * The region is walked as its layout states, the destination first. Along
 * the innermost axis the kernel is called in a loop of its own, which is
 * what a compiler vectorizes when both sides are contiguous there.
 *
 * @tparam Kernel Function called as @c kernel(destination,source), with a
 * pointer to one element of each side.
 * @tparam Destination Pointer to the elements of the destination.
 * @tparam Source Pointer to the elements of the source.
 * @param kernel The function to apply.
 * @param layout The space the region is walked in.
 * @param destination The first element of the region in the destination.
 * @param source The first element of the region in the source.
 */
template <typename Kernel, typename Destination, typename Source>
void run_region_loop(
	const Kernel &kernel,
	const image_region_layout &layout,
	Destination destination,
	Source source
);

} // namespace vitrio

#include "image_region_loop.inl"
