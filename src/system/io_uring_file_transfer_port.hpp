// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "event_counter.hpp"
#include "file_transfer_port.hpp"
#include "io_uring_file_transfer_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vitrio
{

/**
 * @brief A port whose reads the kernel carries out through an io_uring, and
 * which is woken through a read of an @ref event_counter in the same ring.
 *
 * That read is always outstanding while the port lives: waking the port
 * signals the counter, which completes it, and the port queues it again as
 * it returns. Destroying the port signals the counter too, so that its last
 * read completes.
 *
 * A port is not used in the child of a fork.
 */
class io_uring_file_transfer_port final
	: public file_transfer_port
{
public:
	/**
	 * @brief Open a ring that has room for a number of reads, besides the
	 * one the port is woken through.
	 *
	 * More reads may be outstanding than the ring has room for: queueing one
	 * then waits for an earlier one to complete first.
	 *
	 * @param capacity How many reads the kernel may carry out at once. Not
	 * zero.
	 * @throws std::system_error If the kernel can not open the ring or the
	 * counter.
	 */
	explicit io_uring_file_transfer_port(std::size_t capacity);
	~io_uring_file_transfer_port() override;

	void queue_read(
		file_handle_t file,
		std::uint64_t offset,
		span<const memory_range> buffers,
		std::uint64_t tag
	) override;

	void wake() noexcept override;
	boost::optional<file_transfer_completion> wait() override;

private:
	void queue_wakeup();

	event_counter m_wakeups;

	// Where the read of the counter puts its value.
	std::uint64_t m_wakeup_count;

	// The tag each read was queued with, at the tag the ring knows it by.
	// The first stands for the read of the counter.
	std::vector<std::uint64_t> m_tags;
	std::vector<std::uint64_t> m_free_tags;

	// Destroyed first, while the counter and its buffer are still there.
	io_uring_file_transfer_queue m_queue;
};

} // namespace vitrio
