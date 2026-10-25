// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <cstddef>

namespace vitrio
{

class memory_range;

/**
 * @brief A stretch of memory that is only read: where it starts and how many
 * bytes it covers.
 *
 * The counterpart of @ref memory_range for memory that is not written
 * through, such as what a write takes its bytes from. Every @ref memory_range
 * is one.
 */
class VITRIO_API const_memory_range
{
public:
	/**
	 * @brief Construct a stretch from its first byte and its size.
	 *
	 * @param address First byte of the stretch.
	 * @param size How many bytes it covers.
	 */
	const_memory_range(const void *address, std::size_t size) noexcept;

	/**
	 * @brief Construct the stretch a @ref memory_range covers.
	 *
	 * @param range The stretch.
	 */
	const_memory_range(const memory_range &range) noexcept;

	const_memory_range(const const_memory_range &other) = default;
	const_memory_range(const_memory_range &&other) noexcept = default;
	~const_memory_range() = default;

	const_memory_range& operator=(const const_memory_range &other) = default;
	const_memory_range&
	operator=(const_memory_range &&other) noexcept = default;

	/**
	 * @brief Get the first byte of the stretch.
	 *
	 * @return const void* Its address.
	 */
	const void* get_address() const noexcept;

	/**
	 * @brief Get how many bytes the stretch covers.
	 *
	 * @return std::size_t The size in bytes.
	 */
	std::size_t get_size() const noexcept;

private:
	const void *m_address;
	std::size_t m_size;
};

} // namespace vitrio
