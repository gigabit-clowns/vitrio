// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>

namespace vitrio
{

/**
 * @brief A stretch of a file: where it starts and how many bytes it covers.
 *
 * It names bytes of the file by their offset from its first, wherever the
 * file is read through.
 */
class file_range
{
public:
	/**
	 * @brief Construct a stretch from its first byte and its size.
	 *
	 * @param offset Offset of its first byte, from the first of the file.
	 * @param size How many bytes it covers.
	 */
	file_range(std::size_t offset, std::size_t size) noexcept;

	file_range(const file_range &other) = default;
	file_range(file_range &&other) noexcept = default;
	~file_range() = default;

	file_range& operator=(const file_range &other) = default;
	file_range& operator=(file_range &&other) noexcept = default;

	/**
	 * @brief Get the offset of the first byte of the stretch.
	 *
	 * @return std::size_t The offset, in bytes from the first of the file.
	 */
	std::size_t get_offset() const noexcept;

	/**
	 * @brief Get how many bytes the stretch covers.
	 *
	 * @return std::size_t The size in bytes.
	 */
	std::size_t get_size() const noexcept;

	/**
	 * @brief Get the offset of the byte after the stretch.
	 *
	 * @return std::size_t The offset, in bytes from the first of the file.
	 */
	std::size_t get_end() const noexcept;

private:
	std::size_t m_offset;
	std::size_t m_size;
};

} // namespace vitrio
