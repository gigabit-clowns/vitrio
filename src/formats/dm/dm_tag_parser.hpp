// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "dm_header.hpp"
#include "dm_tag.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

namespace vitrio
{
namespace dm
{

/**
 * @brief Parse the tree of tags of a DigitalMicrograph file.
 *
 * The tree is the root group, which follows the header, and everything it
 * holds. A group states its count of entries and a datum the encoding of its
 * value, and nothing else gives the size of either, so the tree is walked
 * from its first byte and the size of each value worked out from its
 * encoding (@ref get_encoded_size) to skip it. Version 4 also states the
 * size of each entry, which is not needed and is not relied on.
 *
 * Every count and size is checked against what is left of the file before it
 * is used, so a damaged file is reported rather than read past its end.
 *
 * @param file The whole of the file.
 * @param header The header of the file, parsed from its first bytes.
 * @return dm_tag The root group, labelled with an empty label.
 * @throws image_file_format_error If the tree does not fit in the file, if an
 * entry is of a kind the format does not have, if a datum lacks its
 * delimiter or has an encoding @ref get_encoded_size refuses, or if groups
 * are nested deeper than @ref max_group_depth.
 */
dm_tag parse_tags(span<const byte> file, const dm_header &header);

} // namespace dm
} // namespace vitrio
