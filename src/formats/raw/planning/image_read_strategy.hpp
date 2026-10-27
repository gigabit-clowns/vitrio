// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_transfer_schedule.hpp"

#include <vitrio/array/numerical_type.hpp>

#include <cstddef>

namespace vitrio
{

class image_region_read_walk;
class image_transfer_plan;
class raw_image_layout;

/**
 * @brief Tell whether the regions of a read lie in the file as they are to
 * lie in the array.
 *
 * They do when they are one value after another on both sides, in the data
 * type and byte order the array takes. Such regions are read straight into
 * the array, and any others through staging, where they are converted.
 *
 * @param walk The regions of the read and the space they are walked in.
 * @param array_type Data type of the array.
 * @param layout Where and how the file holds its values.
 * @return bool Whether they lie as they land.
 */
bool lie_as_they_land(
	const image_region_read_walk &walk,
	numerical_type array_type,
	const raw_image_layout &layout
) noexcept;

/**
 * @brief Work out the stretches of a file that a staged read reads, and the
 * steps they fall in.
 *
 * The stretches of the file the regions reach are merged across the gaps
 * that are narrower than a region, capped at @ref get_prefetch_gap_cap, and
 * start on the grid of the elements of the values, so that each one is
 * staged and converted from its first element.
 *
 * @param regions The regions of the read.
 * @param walk The walk of @p regions, whose file offsets are ascending.
 * @param layout Where and how the file holds its values.
 * @param file_size How many bytes the file has, which no stretch reaches
 * past.
 * @param byte_budget Most bytes a step stages.
 * @return image_transfer_schedule The stretches and their steps.
 */
image_transfer_schedule schedule_staged_read(
	const image_transfer_plan &regions,
	const image_region_read_walk &walk,
	const raw_image_layout &layout,
	std::size_t file_size,
	std::size_t byte_budget
);

} // namespace vitrio
