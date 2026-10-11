// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_descriptor.hpp>

#include <memory/byte_order.hpp>

#include <cstddef>

namespace vitrio
{
namespace dm
{

/**
 * @brief Where the values of the image of a DigitalMicrograph file lie, and
 * what they are.
 *
 * The values are contiguous, the columns changing fastest, and begin
 * wherever the tags before them end: they need not be aligned for their data
 * type.
 */
class dm_image_location
{
public:
	/**
	 * @brief Construct the location of an image.
	 *
	 * @param descriptor The shape and data type of the image.
	 * @param data_offset Where its values begin in the file, in bytes.
	 * @param order The byte order of its values.
	 */
	dm_image_location(
		image_descriptor descriptor,
		std::size_t data_offset,
		byte_order order
	) noexcept;

	dm_image_location(const dm_image_location &other) = default;
	dm_image_location(dm_image_location &&other) noexcept = default;
	~dm_image_location() = default;

	dm_image_location& operator=(const dm_image_location &other) = default;
	dm_image_location&
	operator=(dm_image_location &&other) noexcept = default;

	/**
	 * @brief Get the shape and data type of the image.
	 *
	 * @return const image_descriptor& The descriptor.
	 */
	const image_descriptor& get_descriptor() const noexcept;

	/**
	 * @brief Get where the values of the image begin in the file.
	 *
	 * @return std::size_t The offset, in bytes.
	 */
	std::size_t get_data_offset() const noexcept;

	/**
	 * @brief Get the byte order of the values of the image.
	 *
	 * @return byte_order The order.
	 */
	byte_order get_byte_order() const noexcept;

private:
	image_descriptor m_descriptor;
	std::size_t m_data_offset;
	byte_order m_byte_order;
};

} // namespace dm
} // namespace vitrio
