// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "mrc_header.hpp"
#include "mrc_single_section.hpp"

#include <vitrio/image_metadata.hpp>
#include <vitrio/image_metadata_field.hpp>

#include <string>

namespace vitrio
{
namespace mrc
{

/**
 * @brief The labels of an MRC file, in order, one a line.
 *
 * A file holds up to @ref label_count of them, each of printable ASCII and
 * up to @ref size::label characters, and software records in them what it
 * did to the file.
 */
constexpr image_metadata_field<std::string> labels_field{"mrc.labels"};

/**
 * @brief Read what a header states beyond the shape and data type of its
 * file.
 *
 * The distance between the samples along an axis is the size of the cell
 * along the axis of space it runs along over the sampling of the cell
 * along it. It is stated for the axes of one image or volume, as
 * @ref derive_core_space_axes resolves them, and only where the size and
 * the sampling are both above zero: a header of zeros states none.
 *
 * The labels are stated under @ref labels_field, unless the header has none.
 *
 * @param header The header of the file.
 * @param single_section What the file holds when the header states a single
 * section in the image space group.
 * @return image_metadata What the header states.
 * @throws image_file_format_error If the axis correspondence of the header
 * names anything but the three axes of space, one each, without being unset.
 */
image_metadata derive_metadata(
	const mrc_header &header,
	mrc_single_section single_section = mrc_single_section::image
);

/**
 * @brief Write into a header what metadata states of its file.
 *
 * The inverse of @ref derive_metadata. The cell is given, along the axis
 * of space each axis of one image or volume runs along, the size that
 * states the distance between the samples there, and is left as it is
 * along an axis no distance is stated for. The lines of @ref labels_field
 * are appended to the labels of the header.
 *
 * What a header can not hold is left out, and nothing is refused: a
 * distance that is not finite and above zero or whose cell a @c float does
 * not hold, a line that is no label, the labels past the last a header has
 * room for, and everything under any other label.
 *
 * @param metadata What is stated of the file.
 * @param single_section What the file holds when the header states a single
 * section in the image space group.
 * @param header The header of the file, with its shape already stated.
 * @throws image_file_format_error If the axis correspondence of the header
 * names anything but the three axes of space, one each, without being unset.
 */
void apply_metadata(
	const image_metadata &metadata,
	mrc_single_section single_section,
	mrc_header &header
);

} // namespace mrc
} // namespace vitrio
