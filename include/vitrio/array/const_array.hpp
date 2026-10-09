// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/export.hpp>
#include <vitrio/span.hpp>

#include <memory>

namespace vitrio
{

class array_implementation;

/**
 * @brief A multidimensional array of numbers that cannot be written through.
 *
 * It is an @ref array whose elements can only be read: it keeps its memory
 * alive in the same way, and it cannot be copied either. The memory may be
 * read-only, or it may be the memory of an @ref array that is still written
 * through elsewhere.
 *
 * @see array
 * @see const_array_ref
 */
class const_array
{
public:
	/**
	 * @brief Construct an array that holds nothing.
	 *
	 * Its descriptor is not initialized and it has no memory.
	 */
	VITRIO_API
	const_array() noexcept;

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
	const_array(
		span<const byte> memory,
		std::shared_ptr<const void> owner,
		array_descriptor descriptor
	);

	const_array(const const_array &other) = delete;
	VITRIO_API
	const_array(const_array &&other) noexcept;
	VITRIO_API
	~const_array();

	const_array& operator=(const const_array &other) = delete;
	VITRIO_API
	const_array& operator=(const_array &&other) noexcept;

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
	 * @return const byte* The first byte of the memory. The element at the
	 * origin is at the offset the descriptor states. Null when the array
	 * holds nothing.
	 */
	VITRIO_API
	const byte* get_data() const noexcept;

	/**
	 * @brief Make another read-only array over the same memory.
	 *
	 * @return const_array An array with the same descriptor and the same
	 * elements.
	 */
	VITRIO_API
	const_array share() const noexcept;

private:
	friend class array;

	explicit const_array(
		std::shared_ptr<const array_implementation> implementation
	) noexcept;

	std::shared_ptr<const array_implementation> m_implementation;
};

} // namespace vitrio
