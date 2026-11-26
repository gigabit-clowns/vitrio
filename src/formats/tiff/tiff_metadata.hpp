// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "tiff_page_tags.hpp"

#include <vitrio/image_metadata.hpp>
#include <vitrio/image_metadata_field.hpp>

#include <string>

namespace vitrio
{
namespace tiff
{

/// What a TIFF file says it shows: its ImageDescription tag.
constexpr image_metadata_field<std::string>
image_description_field{"tiff.image_description"};

/// The software a TIFF file says made it: its Software tag.
constexpr image_metadata_field<std::string> software_field{"tiff.software"};

/**
 * @brief When a TIFF file says it was made: its DateTime tag, which the
 * format has as <tt>YYYY:MM:DD HH:MM:SS</tt>.
 */
constexpr image_metadata_field<std::string> date_time_field{"tiff.date_time"};

/**
 * @brief Read what the tags of a page state of its file.
 *
 * The distance between the samples along an axis is the length over how
 * many pixels the page says there are to it, where it says so. The texts
 * are stated under @ref image_description_field, @ref software_field and
 * @ref date_time_field, each unless the page has none.
 *
 * @param tags The tags of the first page of the file.
 * @return image_metadata What they state.
 */
image_metadata derive_metadata(const tiff_page_tags &tags);

/**
 * @brief Make the tags that state what metadata states of a file.
 *
 * The inverse of @ref derive_metadata.
 *
 * What a page can not hold is left out, and nothing is refused: a distance
 * that is not finite and above zero or whose resolution the tag does not
 * hold, a text that is empty or holds a null character, a date that is not
 * as the format has it, and everything under any other label.
 *
 * @param metadata What is stated of the file.
 * @return tiff_page_tags The tags.
 */
tiff_page_tags make_page_tags(const image_metadata &metadata);

} // namespace tiff
} // namespace vitrio
