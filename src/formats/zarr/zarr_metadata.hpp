// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_multiscale.hpp"

#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief Read what an image of OME-Zarr states of one of its levels.
 *
 * The distance between the samples along an axis is the scale of the
 * level along it, times the scale the image states for every level where
 * it states one, in the unit of the axis. It is stated for the axes of one
 * image or volume of the level, and of those only for an axis of space
 * whose unit is a length this format knows: an axis with no unit states a
 * scale and no distance.
 *
 * @param image The image.
 * @param level Which of its levels. Must be one of them.
 * @param descriptor What the array of that level holds.
 * @return image_metadata What the image states of the level. Nothing when
 * the image does not state as many axes, or the level as many scales, as
 * the array has.
 */
image_metadata derive_metadata(
	const zarr_multiscale &image,
	std::size_t level,
	const image_descriptor &descriptor
);

/**
 * @brief Get the distance between the samples along each axis of an array
 * that metadata states.
 *
 * The inverse of @ref derive_metadata, as far as an image of one level
 * goes.
 *
 * @param metadata What is stated of the array.
 * @param descriptor What the array holds.
 * @return std::vector<double> The distance along each axis of
 * @p descriptor, in angstroms, or zero along an axis none is stated for:
 * an axis that is not of one image or volume, and one whose distance is
 * not finite and above zero.
 */
std::vector<double> derive_pixel_sizes(
	const image_metadata &metadata,
	const image_descriptor &descriptor
);

} // namespace zarr
} // namespace vitrio
