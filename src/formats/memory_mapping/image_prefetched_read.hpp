// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <memory/byte_order.hpp>

namespace vitrio
{

class array_descriptor;
class array_ref;

class image_transfer_plan;

/**
 * @brief Read regions of values out of memory, asking for what each step
 * touches before the step ahead of it is walked.
 *
 * The regions are resolved, and so bounds checked, before any of them is
 * asked for. The stretches of @p memory they touch are then worked out and
 * grouped into steps, as @ref image_read_schedule describes, under the
 * policy @ref make_prefetch_policy gives regions of their span. The first
 * step is asked for before any is walked, and each one after it is asked for
 * before the one ahead of it is walked, so that it is on its way while those
 * values are being moved.
 *
 * What asking for a step does is up to @p prefetch, which is given the
 * stretches of one step at a time, and only stretches that lie within
 * @p memory. Whatever it does, the values that land are the same.
 *
 * @tparam Prefetch Function called as @c prefetch(ranges), with the
 * stretches of a step as a @c span<const memory_range>.
 * @param memory The bytes the values lie in, from the first one. No stretch
 * asked for reaches outside of them.
 * @param values How @p memory is read as an array of the values, which the
 * file side of the plan addresses. Every element it addresses lies within
 * @p memory, and its strides are not negative.
 * @param order Byte order the values are stated in.
 * @param destination The array the regions land in.
 * @param regions The regions to read and where each one lands.
 * @param prefetch What asks for the stretches of a step.
 * @throws std::invalid_argument If @p destination is not initialized, or if
 * a rank of the plan does not match the values or @p destination.
 * @throws std::out_of_range If a region is not contained in the values, or
 * does not fit in @p destination where it is placed.
 * @throws unsupported_operation_error If the data type of @p destination
 * can not be produced from the one of the values.
 */
template <typename Prefetch>
void read_prefetched_regions(
	span<const byte> memory,
	const array_descriptor &values,
	byte_order order,
	array_ref destination,
	const image_transfer_plan &regions,
	Prefetch &&prefetch
);

} // namespace vitrio

#include "image_prefetched_read.inl"
