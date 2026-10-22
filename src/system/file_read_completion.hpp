// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <system_error>

namespace vitrio
{

/**
 * @brief How one read of a @ref file_read_queue ended: how many bytes it
 * read, or the error the system failed it with.
 */
class file_read_completion
{
public:
	/**
	 * @brief Construct the completion of a read that succeeded.
	 *
	 * @param tag The tag the read was queued with.
	 * @param size How many bytes it read.
	 */
	file_read_completion(std::uint64_t tag, std::size_t size) noexcept;

	/**
	 * @brief Construct the completion of a read the system failed.
	 *
	 * @param tag The tag the read was queued with.
	 * @param error What the system failed it with.
	 */
	file_read_completion(std::uint64_t tag, std::error_code error) noexcept;

	file_read_completion(const file_read_completion &other) = default;
	file_read_completion(file_read_completion &&other) noexcept = default;
	~file_read_completion() = default;

	file_read_completion&
	operator=(const file_read_completion &other) = default;
	file_read_completion&
	operator=(file_read_completion &&other) noexcept = default;

	/**
	 * @brief Get the tag the read was queued with.
	 *
	 * @return std::uint64_t The tag.
	 */
	std::uint64_t get_tag() const noexcept;

	/**
	 * @brief Get how many bytes the read put in its buffers, from the first.
	 *
	 * @return std::size_t The size in bytes. Zero for a read that failed.
	 */
	std::size_t get_size() const noexcept;

	/**
	 * @brief Get what the system failed the read with.
	 *
	 * @return const std::error_code& The error. Holds none for a read that
	 * succeeded.
	 */
	const std::error_code& get_error() const noexcept;

private:
	std::uint64_t m_tag;
	std::size_t m_size;
	std::error_code m_error;
};

} // namespace vitrio
