// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <system_error>

namespace vitrio
{

/**
 * @brief How one transfer of a @ref file_transfer_submitter ended: how many
 * bytes it read or wrote, or the error the system failed it with.
 */
class file_transfer_completion
{
public:
	/**
	 * @brief Construct the completion of a transfer that succeeded.
	 *
	 * @param tag The tag the transfer was queued with.
	 * @param size How many bytes it read or wrote.
	 */
	file_transfer_completion(std::uint64_t tag, std::size_t size) noexcept;

	/**
	 * @brief Construct the completion of a transfer the system failed.
	 *
	 * @param tag The tag the transfer was queued with.
	 * @param error What the system failed it with.
	 */
	file_transfer_completion(
		std::uint64_t tag,
		std::error_code error
	) noexcept;

	file_transfer_completion(const file_transfer_completion &other) = default;
	file_transfer_completion(
		file_transfer_completion &&other
	) noexcept = default;
	~file_transfer_completion() = default;

	file_transfer_completion&
	operator=(const file_transfer_completion &other) = default;
	file_transfer_completion&
	operator=(file_transfer_completion &&other) noexcept = default;

	/**
	 * @brief Get the tag the transfer was queued with.
	 *
	 * @return std::uint64_t The tag.
	 */
	std::uint64_t get_tag() const noexcept;

	/**
	 * @brief Get how many bytes the transfer moved between the file and its
	 * buffers, from the first.
	 *
	 * @return std::size_t The size in bytes. Zero for a transfer that failed,
	 * and for a sync.
	 */
	std::size_t get_size() const noexcept;

	/**
	 * @brief Get what the system failed the transfer with.
	 *
	 * @return const std::error_code& The error. Holds none for a transfer
	 * that succeeded.
	 */
	const std::error_code& get_error() const noexcept;

private:
	std::uint64_t m_tag;
	std::size_t m_size;
	std::error_code m_error;
};

} // namespace vitrio
