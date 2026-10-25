// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_file_access.hpp"

#include <vitrio/byte.hpp>

#include <system/file_handle.hpp>

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include <cstddef>
#include <string>

namespace vitrio
{

/**
 * @brief One file, held in memory.
 *
 * The whole file is mapped from its first byte, header included. Where the
 * values begin is not a page boundary, and a mapping may only start at one,
 * so the header is mapped along with them and skipped by pointer arithmetic
 * instead.
 *
 * A mapped file is addressed like memory, so a region of it can be walked
 * as a strided operand like any other, with no buffer in between and no
 * system call per region.
 */
class image_file_mapping
{
public:
	/**
	 * @brief Map a file that already exists.
	 *
	 * @param path Path to the file.
	 * @param access Whether the mapping may be written through. Writing
	 * needs a file opened for both reading and writing.
	 * @throws image_file_error If the file can not be opened or mapped, or
	 * if it is empty.
	 */
	image_file_mapping(const std::string &path, image_file_access access);

	image_file_mapping(const image_file_mapping &other) = delete;
	image_file_mapping(image_file_mapping &&other) noexcept = default;
	~image_file_mapping() = default;

	image_file_mapping& operator=(const image_file_mapping &other) = delete;
	image_file_mapping&
	operator=(image_file_mapping &&other) noexcept = default;

	/**
	 * @brief Get the first byte of the file.
	 *
	 * @return byte* The mapped bytes. Writing through them is only defined
	 * when the mapping was opened for writing.
	 */
	byte* get_data() const noexcept;

	/**
	 * @brief Get how many bytes are mapped.
	 *
	 * @return std::size_t The size of the file.
	 */
	std::size_t get_size() const noexcept;

	/**
	 * @brief Get the file that is mapped.
	 *
	 * @return file_handle_t The file, as the operating system names it. It
	 * stays open for as long as the mapping lives.
	 */
	file_handle_t get_file() const noexcept;

private:
	boost::interprocess::file_mapping m_mapping;
	boost::interprocess::mapped_region m_region;
};

} // namespace vitrio
