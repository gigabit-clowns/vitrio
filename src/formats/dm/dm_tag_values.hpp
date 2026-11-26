// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "dm_tag.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <memory/byte_order.hpp>

#include <cstdint>
#include <string>

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

/**
 * @brief Check whether an entry is a datum that holds a single real.
 *
 * @param tag The entry.
 * @return bool true if @ref read_real reads it.
 */
bool holds_real(const dm_tag &tag) noexcept;

/**
 * @brief Read the value of a datum that holds a single real, of either
 * width.
 *
 * An integer is no real, and is not read as one.
 *
 * @param tag The datum, from the tree @ref parse_tags made of @p file.
 * @param file The whole of the file.
 * @param order The byte order of the values of the file.
 * @return double The value.
 * @throws image_file_format_error If @p tag does not hold a single real.
 */
double read_real(
	const dm_tag &tag,
	span<const byte> file,
	byte_order order
);

/**
 * @brief Check whether an entry is a datum that holds a text as the format
 * writes it, an array of 16-bit characters.
 *
 * @param tag The entry.
 * @return bool true if @ref read_text reads it.
 */
bool holds_text(const dm_tag &tag) noexcept;

/**
 * @brief Read the value of a datum that holds a text.
 *
 * @param tag The datum, from the tree @ref parse_tags made of @p file.
 * @param file The whole of the file.
 * @param order The byte order of the values of the file.
 * @return std::u16string The text, in UTF-16 as the file holds it, in the
 * byte order of the host.
 * @throws image_file_format_error If @p tag does not hold a text.
 */
std::u16string read_text(
	const dm_tag &tag,
	span<const byte> file,
	byte_order order
);

} // namespace dm
} // namespace vitrio
