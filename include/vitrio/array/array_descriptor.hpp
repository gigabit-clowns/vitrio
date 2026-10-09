// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/export.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

/**
 * @brief How a block of memory is read as a multidimensional array.
 *
 * A descriptor states the extents of the array, where each of its elements
 * is within the memory, and the type of the elements. It does not refer to
 * any memory itself.
 *
 * The element at index @c (i0,i1,...) is
 * @c offset+i0*strides[0]+i1*strides[1]+... elements after the first byte of
 * the memory. Strides and the offset are counted in elements, not in bytes.
 * A stride may be negative, as long as the offset keeps every element at or
 * after the first byte.
 *
 * @see make_contiguous_array_descriptor
 */
class array_descriptor
{
public:
	/**
	 * @brief Construct a descriptor that describes nothing.
	 *
	 * It has no extents and its data type is unknown.
	 *
	 * @see is_initialized
	 */
	VITRIO_API
	array_descriptor() noexcept;

	/**
	 * @brief Construct a descriptor from its components.
	 *
	 * @param extents Number of elements along each axis, slowest axis first.
	 * @param strides Distance between consecutive elements along each axis,
	 * in elements. There must be one per extent.
	 * @param offset Position of the element at the origin, in elements from
	 * the first byte of the memory.
	 * @param data_type Type of the elements.
	 * @throws std::invalid_argument If @p strides does not have one stride
	 * per extent, or if @p data_type is unknown.
	 */
	VITRIO_API
	array_descriptor(
		std::vector<std::size_t> extents,
		std::vector<std::ptrdiff_t> strides,
		std::ptrdiff_t offset,
		numerical_type data_type
	);

	VITRIO_API
	array_descriptor(const array_descriptor &other);
	VITRIO_API
	array_descriptor(array_descriptor &&other) noexcept;
	VITRIO_API
	~array_descriptor();

	VITRIO_API
	array_descriptor& operator=(const array_descriptor &other);
	VITRIO_API
	array_descriptor& operator=(array_descriptor &&other) noexcept;

	/**
	 * @brief Get the number of elements along each axis.
	 *
	 * @return span<const std::size_t> The extents, slowest axis first. It
	 * refers to storage owned by this descriptor.
	 */
	VITRIO_API
	span<const std::size_t> get_extents() const noexcept;

	/**
	 * @brief Get the distance between consecutive elements along each axis.
	 *
	 * @return span<const std::ptrdiff_t> The strides, in elements and one per
	 * extent. It refers to storage owned by this descriptor.
	 */
	VITRIO_API
	span<const std::ptrdiff_t> get_strides() const noexcept;

	/**
	 * @brief Get the position of the element at the origin.
	 *
	 * @return std::ptrdiff_t The offset, in elements from the first byte of
	 * the memory.
	 */
	VITRIO_API
	std::ptrdiff_t get_offset() const noexcept;

	/**
	 * @brief Get the type of the elements.
	 *
	 * @return numerical_type The data type. Unknown only for a descriptor
	 * that describes nothing.
	 */
	VITRIO_API
	numerical_type get_data_type() const noexcept;

	friend bool operator==(
		const array_descriptor &lhs,
		const array_descriptor &rhs
	) noexcept
	{
		return
			lhs.m_offset == rhs.m_offset &&
			lhs.m_data_type == rhs.m_data_type &&
			lhs.m_extents == rhs.m_extents &&
			lhs.m_strides == rhs.m_strides;
	}

	friend bool operator!=(
		const array_descriptor &lhs,
		const array_descriptor &rhs
	) noexcept
	{
		return !(lhs == rhs);
	}

private:
	std::vector<std::size_t> m_extents;
	std::vector<std::ptrdiff_t> m_strides;
	std::ptrdiff_t m_offset;
	numerical_type m_data_type;
};

/**
 * @brief Make the descriptor of an array whose elements follow one another.
 *
 * The elements are in row-major order: the last axis is the fastest, its
 * stride is one, and the element at the origin is the first of the memory.
 *
 * @param extents Number of elements along each axis, slowest axis first.
 * @param data_type Type of the elements.
 * @return array_descriptor The descriptor.
 * @throws std::invalid_argument If @p data_type is unknown.
 * @throws std::out_of_range If the extents hold more elements than can be
 * addressed.
 */
VITRIO_API
array_descriptor make_contiguous_array_descriptor(
	span<const std::size_t> extents,
	numerical_type data_type
);

/**
 * @brief Check whether a descriptor describes an array.
 *
 * @param descriptor The descriptor.
 * @return true It was constructed from its components.
 * @return false It describes nothing.
 */
VITRIO_API
bool is_initialized(const array_descriptor &descriptor) noexcept;

} // namespace vitrio
