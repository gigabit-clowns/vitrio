// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/span.hpp>

#include <memory/byte_order.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief How the values of one decoded chunk of a ZARR array lie in its
 * bytes.
 *
 * A chunk is decoded into its values in C order over its axes taken in some
 * order: their own for the order C of version 2, the reverse for the order F,
 * and that of the transposes of version 3. The strides say where that puts
 * each axis of the array. Every chunk has the full extents of one, those at
 * the edges of the array too.
 */
class zarr_chunk_layout
{
public:
	/**
	 * @brief Lay a chunk out.
	 *
	 * @param extents Extents of a chunk, along the axes of the array.
	 * @param memory_order The axes of the array from the one that varies
	 * slowest in memory to the fastest. A permutation of the axes.
	 * @param type The data type of the values.
	 * @param order The order of the bytes of each value.
	 * @throws std::invalid_argument If @p memory_order is not a permutation
	 * of the axes of @p extents.
	 * @throws std::overflow_error If the bytes of a chunk can not be
	 * counted.
	 */
	zarr_chunk_layout(
		std::vector<std::size_t> extents,
		span<const std::size_t> memory_order,
		numerical_type type,
		byte_order order
	);

	/**
	 * @brief Get the extents of a chunk.
	 *
	 * @return span<const std::size_t> The extents, along the axes of the
	 * array. It refers to storage owned by this layout.
	 */
	span<const std::size_t> get_extents() const noexcept;

	/**
	 * @brief Get where each axis of the array lies in a decoded chunk.
	 *
	 * @return span<const std::ptrdiff_t> The strides, in elements, along
	 * the axes of the array. It refers to storage owned by this layout.
	 */
	span<const std::ptrdiff_t> get_strides() const noexcept;

	/**
	 * @brief Get the data type of the values.
	 *
	 * @return numerical_type The data type.
	 */
	numerical_type get_data_type() const noexcept;

	/**
	 * @brief Get the order of the bytes of each value.
	 *
	 * @return byte_order The order.
	 */
	byte_order get_byte_order() const noexcept;

	/**
	 * @brief Get how many values a chunk holds.
	 *
	 * @return std::size_t The product of the extents.
	 */
	std::size_t get_element_count() const noexcept;

	/**
	 * @brief Get how many bytes a decoded chunk takes.
	 *
	 * @return std::size_t The size in bytes.
	 */
	std::size_t get_byte_size() const noexcept;

private:
	std::vector<std::size_t> m_extents;
	std::vector<std::ptrdiff_t> m_strides;
	numerical_type m_type;
	byte_order m_order;
	std::size_t m_element_count;
};

} // namespace zarr
} // namespace vitrio
