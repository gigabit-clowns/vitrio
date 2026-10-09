// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/export.hpp>
#include <vitrio/span.hpp>

namespace vitrio
{

class array;

/**
 * @brief A reference to a multidimensional array that does not keep it alive.
 *
 * A reference is two pointers, one to the memory of an array and one to its
 * descriptor. It owns neither, so copying it copies the two pointers and
 * nothing else, as copying a span does.
 *
 * It is the parameter type of a function that uses an array only until it
 * returns. What it refers to must outlive it, so a reference should not be
 * stored.
 *
 * An @ref array converts to a reference to itself.
 *
 * @see array
 * @see const_array_ref
 */
class array_ref
{
public:
	/**
	 * @brief Construct a reference to nothing.
	 *
	 * Its descriptor is not initialized and it has no memory.
	 */
	VITRIO_API
	array_ref() noexcept;

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
	array_ref(span<byte> memory, const array_descriptor &descriptor);

	// A temporary descriptor would be gone before the reference is used.
	array_ref(span<byte> memory, const array_descriptor &&descriptor) = delete;

	/**
	 * @brief Construct a reference to an array.
	 *
	 * @param other The array to refer to. It must outlive the reference and
	 * its copies.
	 */
	VITRIO_API
	array_ref(array &other) noexcept;

	array_ref(const array_ref &other) noexcept = default;
	~array_ref() = default;

	array_ref& operator=(const array_ref &other) noexcept = default;

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
	 * @return byte* The first byte of the memory. The element at the origin
	 * is at the offset the descriptor states. Null when the reference is to
	 * nothing.
	 */
	VITRIO_API
	byte* get_data() const noexcept;

private:
	byte *m_data;
	const array_descriptor *m_descriptor;
};

} // namespace vitrio
