// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/export.hpp>
#include <vitrio/span.hpp>

namespace vitrio
{

class array;
class const_array;

/**
 * @brief A read-only reference to a multidimensional array that does not
 * keep it alive.
 *
 * It is an @ref array_ref whose elements can only be read: two pointers that
 * own nothing and are as cheap to copy. An @ref array, a @ref const_array
 * and an @ref array_ref each convert to it.
 *
 * @see const_array
 * @see array_ref
 */
class const_array_ref
{
public:
	/**
	 * @brief Construct a reference to nothing.
	 *
	 * Its descriptor is not initialized and it has no memory.
	 */
	VITRIO_API
	const_array_ref() noexcept;

	/**
	 * @brief Construct a reference to memory the caller keeps alive.
	 *
	 * @param memory The memory holding the elements. It must outlive the
	 * reference and its copies.
	 * @param descriptor How @p memory is read as an array. It must outlive
	 * the reference and its copies.
	 * @throws std::invalid_argument If @p descriptor is not initialized, or
	 * if @p memory is not aligned to the elements.
	 * @throws std::out_of_range If an element @p descriptor addresses lies
	 * outside @p memory.
	 */
	VITRIO_API
	const_array_ref(
		span<const byte> memory,
		const array_descriptor &descriptor
	);

	// A temporary descriptor would be gone before the reference is used.
	const_array_ref(
		span<const byte> memory,
		const array_descriptor &&descriptor
	) = delete;

	/**
	 * @brief Construct a reference to an array.
	 *
	 * @param other The array to refer to. It must outlive the reference and
	 * its copies.
	 */
	VITRIO_API
	const_array_ref(const array &other) noexcept;

	/**
	 * @brief Construct a reference to a read-only array.
	 *
	 * @param other The array to refer to. It must outlive the reference and
	 * its copies.
	 */
	VITRIO_API
	const_array_ref(const const_array &other) noexcept;

	/**
	 * @brief Construct a reference to what another reference refers to.
	 *
	 * @param other The reference whose array to refer to.
	 */
	VITRIO_API
	const_array_ref(array_ref other) noexcept;

	const_array_ref(const const_array_ref &other) noexcept = default;
	~const_array_ref() = default;

	const_array_ref&
	operator=(const const_array_ref &other) noexcept = default;

	/**
	 * @brief Get how the memory is read as an array.
	 *
	 * @return const array_descriptor& The descriptor. Not initialized when
	 * the reference is to nothing.
	 */
	VITRIO_API
	const array_descriptor& get_descriptor() const noexcept;

	/**
	 * @brief Get the memory holding the elements.
	 *
	 * @return const byte* The first byte of the memory. The element at the
	 * origin is at the offset the descriptor states. Null when the reference
	 * is to nothing.
	 */
	VITRIO_API
	const byte* get_data() const noexcept;

private:
	const byte *m_data;
	const array_descriptor *m_descriptor;
};

} // namespace vitrio
