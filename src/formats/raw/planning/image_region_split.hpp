// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/span.hpp>

#include <cstddef>

namespace vitrio
{

/**
 * @brief Split the regions of a plan into parts that lie close together in a
 * file, where their parts lie far apart.
 *
 * A stretch of the file read for a region reaches from its first byte to its
 * last, so a region whose parts lie far apart, such as a slice across the
 * planes of a volume, would be read with all that lies between them. Its
 * axes are gathered from the one the file lays out fastest: an axis joins
 * those gathered so far while the gaps it leaves between their parts are no
 * wider than what those parts span, floored at a page and capped at
 * @p gap_cap, and while a part with gaps in it stays within @p byte_budget.
 * Every region is split along every axis that does not join, the slower ones
 * included, into parts one position deep along them.
 *
 * Regions whose values follow one another are never split, whatever their
 * size.
 *
 * @param regions The regions.
 * @param file_strides Distance between consecutive elements of the file
 * along each of its axes, in elements. None is negative.
 * @param data_type Data type of the elements of the file.
 * @param page_size The least gap that is always bridged, in bytes.
 * @param gap_cap The widest gap that is ever bridged, in bytes.
 * @param byte_budget The most bytes a part with gaps in it spans, unless a
 * single position along its slowest axis spans more.
 * @return image_transfer_plan The parts, of the ranks of the plan, each
 * where its region placed it, in the order of the regions. A copy of the plan
 * where no region is split.
 */
image_transfer_plan split_scattered_regions(
	const image_transfer_plan &regions,
	span<const std::ptrdiff_t> file_strides,
	numerical_type data_type,
	std::size_t page_size,
	std::size_t gap_cap,
	std::size_t byte_budget
);

} // namespace vitrio
