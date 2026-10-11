// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>

namespace vitrio
{
namespace dm
{

/**
 * @brief Get the size of a value of a scalar type of the format.
 *
 * @param type The type, one of those of the `encoding` namespace.
 * @return std::size_t The size, in bytes, or zero if @p type is not a
 * scalar type.
 */
std::size_t get_scalar_size(std::uint64_t type) noexcept;

/**
 * @brief Check whether a type of the format is an integer, signed or not.
 *
 * Booleans are not.
 *
 * @param type The type, one of those of the `encoding` namespace.
 * @return bool true if @p type is an integer type.
 */
bool is_integer(std::uint64_t type) noexcept;

/**
 * @brief Work out the size of the value of a data entry from its encoding.
 *
 * An encoding is a sequence of words that names a type, followed by what
 * that type is made of:
 * - a scalar type is the word alone;
 * - a string is followed by its length in bytes, a byte to a character, as
 *   the readers of the format have it (the text the format writes itself is
 *   an array of 16-bit characters instead);
 * - a structure is followed by the length of its name, its count of fields
 *   and, for each field, the length of its name and its type, which is a
 *   scalar;
 * - an array is followed by the encoding of its element and then by its
 *   length.
 *
 * The lengths of names are those of names the format never writes, and are
 * ignored.
 *
 * @param encoding The words of the encoding.
 * @return std::size_t The size of the value, in bytes.
 * @throws image_file_format_error If the encoding names a type the format
 * does not have, if it ends before the type it names does or holds words
 * past it, if it nests arrays deeper than @ref max_encoding_depth, or if the
 * size does not fit in std::ptrdiff_t.
 */
std::size_t get_encoded_size(span<const std::uint64_t> encoding);

} // namespace dm
} // namespace vitrio
