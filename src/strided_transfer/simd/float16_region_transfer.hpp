// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <strided_transfer/region_loop/region_layout.hpp>

#include <vitrio/span.hpp>

#include <array/float16_t.hpp>

#include <cstddef>

namespace vitrio
{

/**
 * @brief Walk every region of a transfer, widening half precision values
 * into floats with the vector units of the processor.
 *
 * The regions are walked as @c run_contiguous_regions walks them, with the
 * same arguments: the innermost axis of the layout is contiguous on both
 * sides, as a loop over the vector units wants it, and the caller sees to
 * that. The loop is compiled once per instruction set the build asks for,
 * and the one for the processor this runs on is chosen once, here, before
 * the first region.
 *
 * Every finite value and infinity is the float @ref float16_t widens it to,
 * bit for bit. A NaN stays a NaN of the same sign, quieted; whether its
 * payload is kept depends on the instruction set.
 *
 * @pre @c layout.is_inner_axis_contiguous()
 * @param layout The space every region is walked in, the destination
 * first.
 * @param destination_offsets Where each region starts in the destination,
 * in elements from its first.
 * @param source_offsets Where each region starts in the source, in elements
 * from its first.
 * @param destination The element of the destination at
 * @p destination_origin.
 * @param destination_origin The offset @p destination holds.
 * @param source The element of the source at @p source_origin.
 * @param source_origin The offset @p source holds.
 */
void widen_float16_regions(
	const region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	float *destination,
	std::ptrdiff_t destination_origin,
	const float16_t *source,
	std::ptrdiff_t source_origin
);

/**
 * @brief Walk every region of a transfer, narrowing floats into half
 * precision values with the vector units of the processor.
 *
 * Every finite value and infinity becomes the half precision number
 * @ref float16_t makes of it, bit for bit: rounded to nearest, ties to
 * even, and an infinity from 65520 on. A NaN stays a NaN of the same sign,
 * quieted; whether its payload is kept depends on the instruction set.
 *
 * @see widen_float16_regions for the arguments.
 */
void narrow_float16_regions(
	const region_layout &layout,
	span<const std::ptrdiff_t> destination_offsets,
	span<const std::ptrdiff_t> source_offsets,
	float16_t *destination,
	std::ptrdiff_t destination_origin,
	const float *source,
	std::ptrdiff_t source_origin
);

} // namespace vitrio
