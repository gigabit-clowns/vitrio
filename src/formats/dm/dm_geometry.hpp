// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "dm_image_location.hpp"
#include "dm_tag.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <memory/byte_order.hpp>

namespace vitrio
{
namespace dm
{

/**
 * @brief Find the image a DigitalMicrograph file holds, and how its values
 * are laid out.
 *
 * The images of a file are the unlabelled groups of its `ImageList`, and
 * those the `Thumbnails` group names by position are previews written for
 * the file browser. The image of the file is the first of the others, which
 * is the one DigitalMicrograph shows, and any after it are not read.
 *
 * Its `ImageData` group states the code of its data type in `DataType`, its
 * extents in `Dimensions`, columns first, and holds its values in `Data`.
 * Two extents make an image and three a stack of images, the first axis of
 * the descriptor running along the last extent of the file.
 *
 * @param root The root group of the file, as @ref parse_tags made it.
 * @param file The whole of the file.
 * @param order The byte order of the values of the file.
 * @return dm_image_location Where the values of the image lie.
 * @throws image_file_format_error If the file holds no image but
 * thumbnails, if a group or datum the image is described by is missing or
 * not of the kind it should be, if an extent is negative, or if `Data` is not
 * the size the extents and data type make.
 * @throws unsupported_operation_error If the image is not of two or three
 * axes, or if its data type is one @ref get_data_type does not read.
 */
dm_image_location locate_image(
	const dm_tag &root,
	span<const byte> file,
	byte_order order
);

} // namespace dm
} // namespace vitrio
