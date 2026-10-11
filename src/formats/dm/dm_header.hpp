// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <memory/byte_order.hpp>

#include <cstddef>
#include <cstdint>

namespace vitrio
{
namespace dm
{

/**
 * @brief The header of one DigitalMicrograph file, of version 3 or 4.
 *
 * The header states the version of the file and the byte order of its
 * values. The structure of the file, the header included, is big-endian
 * whatever that order is: only the values of its data entries follow it.
 *
 * The two versions differ in the width of the counts and sizes of the
 * structure, which is 32 bits in version 3 and 64 bits in version 4. That
 * width is called a word here.
 */
class dm_header
{
public:
	/**
	 * @brief Construct the header of a file.
	 *
	 * @param version The version of the file, 3 or 4.
	 * @param order The byte order of the values of the file.
	 */
	dm_header(std::uint32_t version, byte_order order) noexcept;

	dm_header(const dm_header &other) = default;
	dm_header(dm_header &&other) noexcept = default;
	~dm_header() = default;

	dm_header& operator=(const dm_header &other) = default;
	dm_header& operator=(dm_header &&other) noexcept = default;

	/**
	 * @brief Get the version of the file.
	 *
	 * @return std::uint32_t 3 or 4.
	 */
	std::uint32_t get_version() const noexcept;

	/**
	 * @brief Get the byte order of the values of the file.
	 *
	 * @return byte_order The order.
	 */
	byte_order get_byte_order() const noexcept;

	/**
	 * @brief Get the width of the counts and sizes of the structure.
	 *
	 * @return std::size_t 4 for version 3 and 8 for version 4, in bytes.
	 */
	std::size_t get_word_size() const noexcept;

	/**
	 * @brief Get where the root group begins, which is right after the
	 * header.
	 *
	 * @return std::size_t The offset, in bytes.
	 */
	std::size_t get_root_offset() const noexcept;

private:
	std::uint32_t m_version;
	byte_order m_byte_order;
};

/**
 * @brief Check whether some bytes begin as a DigitalMicrograph file of
 * version 3 or 4 does.
 *
 * The version must be 3 or 4, the byte order one of the two the format
 * states, and the two flags the root group begins with each 0 or 1. The
 * version is a 32-bit field that begins with three zeros, so the check is a
 * weak one and an extension is wanted to back it.
 *
 * @param bytes The leading bytes of the file.
 * @return bool true if they begin as such a file does.
 */
bool has_dm_signature(span<const byte> bytes) noexcept;

/**
 * @brief Parse the header of a DigitalMicrograph file.
 *
 * @param bytes The leading bytes of the file.
 * @return dm_header The header.
 * @throws image_file_format_error If the bytes are too few to hold the
 * header, if the version is neither 3 nor 4, or if the byte order is not one
 * of the two the format states.
 */
dm_header parse_header(span<const byte> bytes);

} // namespace dm
} // namespace vitrio
