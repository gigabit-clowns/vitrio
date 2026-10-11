// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "dm_tag.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <memory/byte_order.hpp>

#include <cstdint>

namespace vitrio
{
namespace dm
{

/**
 * @brief Read the value of a datum that holds a single integer.
 *
 * Any width and signedness of integer is read, since the files do not agree
 * on which they state a given value in: an extent is 32 bits in some and 64
 * in others.
 *
 * @param tag The datum, from the tree @ref parse_tags made of @p file.
 * @param file The whole of the file.
 * @param order The byte order of the values of the file.
 * @return std::int64_t The value.
 * @throws image_file_format_error If @p tag is a group or does not hold a
 * single integer, or if its value does not fit in std::int64_t.
 */
std::int64_t read_integer(
	const dm_tag &tag,
	span<const byte> file,
	byte_order order
);

} // namespace dm
} // namespace vitrio
