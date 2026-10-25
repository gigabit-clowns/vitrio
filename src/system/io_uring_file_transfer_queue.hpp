// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_queue.hpp"

#include <liburing.h>

#include <sys/types.h>
#include <sys/uio.h>

#include <cstddef>
#include <deque>
#include <vector>

namespace vitrio
{

/**
 * @brief A queue that hands its transfers to the kernel through an io_uring,
 * which carries them out while the thread goes on.
 *
 * A queue the child of a fork inherits, with no transfer outstanding, opens a
 * ring of its own there before it takes a transfer, and leaves its parent's
 * be.
 */
class io_uring_file_transfer_queue final
	: public file_transfer_queue
{
public:
	/**
	 * @brief Open a ring that has room for a number of transfers.
	 *
	 * More transfers may be outstanding than the ring has room for: queueing
	 * one then waits for an earlier one to complete first.
	 *
	 * @param capacity How many transfers the kernel may carry out at once.
	 * Not zero.
	 * @throws std::system_error If the kernel can not open the ring.
	 */
	explicit io_uring_file_transfer_queue(std::size_t capacity);

	~io_uring_file_transfer_queue() override;

	void queue_read(
		file_handle_t file,
		std::uint64_t offset,
		span<const memory_range> buffers,
		std::uint64_t tag
	) override;

	void queue_write(
		file_handle_t file,
		std::uint64_t offset,
		span<const const_memory_range> buffers,
		std::uint64_t tag
	) override;

	void queue_sync(file_handle_t file, std::uint64_t tag) override;

	file_transfer_completion wait() override;

private:
	enum class direction
	{
		read,
		write,
		sync
	};

	// A transfer the kernel is carrying out, which it names by the address
	// of this.
	struct request
	{
		direction kind;
		file_handle_t file;
		std::uint64_t offset;
		std::uint64_t tag;
		// The kernel only reads the buffers of a write.
		std::vector<memory_range> buffers;
		std::size_t done;
		std::vector<iovec> vectors;
	};

	// Takes a request to fill, making room for it first if there is none.
	request& take_request();

	// Hands the kernel the request, or completes it at once if it has
	// nothing to move.
	void dispatch(request &transfer);

	// Hands the kernel what is left of a transfer, and tells whether
	// anything was.
	bool send(request &transfer) noexcept;

	// Waits until the kernel completes a transfer.
	file_transfer_completion receive();

	void submit();

	void open_ring();

	void close_ring() noexcept;

	std::vector<request> m_requests;
	std::vector<request*> m_free_requests;

	// The transfers that completed while the queue made room for others.
	std::deque<file_transfer_completion> m_completions;

	io_uring m_ring;

	// The process the ring was opened in, and none while it is closed.
	pid_t m_process;
};

} // namespace vitrio
