// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/const_array.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/export.hpp>
#include <vitrio/span.hpp>

#include <memory>

namespace vitrio
{

class array_implementation;

/**
 * @brief A multidimensional array of numbers in host memory.
 *
 * An array is a block of memory and the descriptor that says how to read it
 * as a multidimensional array. It keeps the memory alive: the memory is
 * released when the last array sharing it is destroyed.
 *
 * An array cannot be copied, so that two names for the same elements are
 * never made by accident. @ref share makes another array over the same
 * memory on purpose.
 *
 * The array does nothing with its elements. It has no arithmetic, slicing or
 * reshaping.
 *
 * @see const_array
 * @see array_ref
 * @see make_array
 */
class array
{
public:
	/**
	 * @brief Construct an array that holds nothing.
	 *
	 * Its descriptor is not initialized and it has no memory.
	 */
	VITRIO_API
	array() noexcept;

	/**
	 * @brief Construct an array over memory allocated elsewhere.
	 *
	 * The array keeps @p owner for as long as it or any array sharing its
	 * memory lives. Whatever releasing @p owner does is how the memory is
	 * released, so the memory may come from anywhere.
	 *
	 * @param memory The memory holding the elements.
	 * @param owner What keeps @p memory alive. Must not be null.
	 * @param descriptor How @p memory is read as an array.
	 * @throws std::invalid_argument If @p owner is null, if @p descriptor is
	 * not initialized, or if @p memory is not aligned to the elements.
	 * @throws std::out_of_range If an element @p descriptor addresses lies
	 * outside @p memory.
	 */
	VITRIO_API
	array(
		span<byte> memory,
		std::shared_ptr<void> owner,
		array_descriptor descriptor
	);

	array(const array &other) = delete;
	VITRIO_API
	array(array &&other) noexcept;
	VITRIO_API
	~array();

	array& operator=(const array &other) = delete;
	VITRIO_API
	array& operator=(array &&other) noexcept;

	/**
	 * @brief Get how the memory is read as an array.
	 *
	 * @return const array_descriptor& The descriptor. Not initialized when
	 * the array holds nothing.
	 */
	VITRIO_API
	const array_descriptor& get_descriptor() const noexcept;

	/**
	 * @brief Get the memory holding the elements.
	 *
	 * @return byte* The first byte of the memory. The element at the origin
	 * is at the offset the descriptor states. Null when the array holds
	 * nothing.
	 */
	VITRIO_API
	byte* get_data() noexcept;

	/**
	 * @brief Get the memory holding the elements, to read them.
	 *
	 * @return const byte* The first byte of the memory. The element at the
	 * origin is at the offset the descriptor states. Null when the array
	 * holds nothing.
	 */
	VITRIO_API
	const byte* get_data() const noexcept;

	/**
	 * @brief Make another array over the same memory.
	 *
	 * @return array An array with the same descriptor and the same elements.
	 * Writing through either one changes both.
	 */
	VITRIO_API
	array share() noexcept;

	/**
	 * @brief Make a read-only array over the same memory.
	 *
	 * @return const_array An array with the same descriptor and the same
	 * elements, which cannot be written through.
	 */
	VITRIO_API
	const_array share_const() const noexcept;

private:
	explicit array(
		std::shared_ptr<const array_implementation> implementation
	) noexcept;

	std::shared_ptr<const array_implementation> m_implementation;
};

/**
 * @brief Make an array with memory of its own.
 *
 * The memory is aligned to 64 bytes and its elements are not initialized.
 *
 * @param descriptor How the memory is read as an array.
 * @return array The array.
 * @throws std::invalid_argument If @p descriptor is not initialized.
 * @throws std::out_of_range If an element @p descriptor addresses lies before
 * the first byte of the memory.
 * @throws std::bad_alloc If the memory could not be allocated.
 */
VITRIO_API
array make_array(const array_descriptor &descriptor);

} // namespace vitrio
