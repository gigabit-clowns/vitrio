// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "dm_tag.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/span.hpp>

#include <memory/byte_order.hpp>

namespace vitrio
{
namespace dm
{

/**
 * @brief Read what the tags of an image of a DigitalMicrograph file state
 * of it.
 *
 * An image states how it is calibrated along each of its axes in the
 * `Dimension` group of the `Calibrations` of its `ImageData`, the axis of
 * its columns first: a `Scale`, the length of a step along the axis, and
 * the `Units` it is in. The distance between the samples along x and y is
 * the scale of the first and of the second, where its units are a length
 * this format knows. An axis with no units is not calibrated, and one in
 * the units of a diffraction pattern or of a spectrum states no distance.
 *
 * An image has no z, and a stack of images none either, whatever the third
 * of its axes is calibrated in.
 *
 * What of this an image lacks, or holds as anything else, is left out, and
 * nothing is refused.
 *
 * @param image The group of the image, as @ref find_image finds it.
 * @param file The whole of the file.
 * @param order The byte order of the values of the file.
 * @return image_metadata What the image states.
 */
image_metadata derive_metadata(
	const dm_tag &image,
	span<const byte> file,
	byte_order order
);

} // namespace dm
} // namespace vitrio
