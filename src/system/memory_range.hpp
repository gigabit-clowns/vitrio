// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>

namespace vitrio
{

/**
 * @brief A stretch of memory: where it starts and how many bytes it covers.
 */
class memory_range
{
public:
	/**
	 * @brief Construct a stretch from its first byte and its size.
	 *
	 * @param address First byte of the stretch.
	 * @param size How many bytes it covers.
	 */
	memory_range(void *address, std::size_t size) noexcept;

	memory_range(const memory_range &other) = default;
	memory_range(memory_range &&other) noexcept = default;
	~memory_range() = default;

	memory_range& operator=(const memory_range &other) = default;
	memory_range& operator=(memory_range &&other) noexcept = default;

	/**
	 * @brief Get the first byte of the stretch.
	 *
	 * @return void* Its address.
	 */
	void* get_address() const noexcept;

	/**
	 * @brief Get how many bytes the stretch covers.
	 *
	 * @return std::size_t The size in bytes.
	 */
	std::size_t get_size() const noexcept;

private:
	void *m_address;
	std::size_t m_size;
};

} // namespace vitrio
