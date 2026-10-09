// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <memory>

namespace vitrio
{

/**
 * @brief What the arrays over the same elements share.
 *
 * It is the memory of an array, what keeps that memory alive, and the
 * descriptor the memory is read with. It never changes once constructed, so
 * an array and the arrays shared from it hold one between them.
 *
 * The memory is held as read-only whatever it was given as. An array that
 * was constructed over writable memory gives it back as writable.
 */
class array_implementation
{
public:
	/**
	 * @brief Construct an implementation from its components.
	 *
	 * @param memory The memory holding the elements.
	 * @param owner What keeps @p memory alive.
	 * @param descriptor How @p memory is read as an array.
	 * @throws std::invalid_argument If @p descriptor is not initialized, or
	 * if @p memory is not aligned to the elements.
	 * @throws std::out_of_range If an element @p descriptor addresses lies
	 * outside @p memory.
	 */
	array_implementation(
		span<const byte> memory,
		std::shared_ptr<const void> owner,
		array_descriptor descriptor
	);

	array_implementation(const array_implementation &other) = delete;
	array_implementation(array_implementation &&other) = delete;
	~array_implementation();

	array_implementation&
	operator=(const array_implementation &other) = delete;
	array_implementation& operator=(array_implementation &&other) = delete;

	/**
	 * @brief Get how the memory is read as an array.
	 *
	 * @return const array_descriptor& The descriptor.
	 */
	const array_descriptor& get_descriptor() const noexcept;

	/**
	 * @brief Get the memory holding the elements.
	 *
	 * @return const byte* The first byte of the memory.
	 */
	const byte* get_data() const noexcept;

	/**
	 * @brief Get the descriptor of an array that holds nothing.
	 *
	 * @return const array_descriptor& A descriptor that is not initialized.
	 */
	static const array_descriptor& get_empty_descriptor() noexcept;

private:
	span<const byte> m_memory;
	std::shared_ptr<const void> m_owner;
	array_descriptor m_descriptor;
};

} // namespace vitrio
