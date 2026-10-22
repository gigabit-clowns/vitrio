// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_queue.hpp"

#include <liburing.h>

#include <sys/uio.h>

#include <cstddef>
#include <deque>
#include <vector>

namespace vitrio
{

/**
 * @brief A queue that hands its reads to the kernel through an io_uring,
 * which carries them out while the thread goes on.
 */
class io_uring_file_read_queue final
	: public file_read_queue
{
public:
	/**
	 * @brief Open a ring that has room for a number of reads.
	 *
	 * More reads may be outstanding than the ring has room for: queueing one
	 * then waits for an earlier one to complete first.
	 *
	 * @param capacity How many reads the kernel may carry out at once. Not
	 * zero.
	 * @throws std::system_error If the kernel can not open the ring.
	 */
	explicit io_uring_file_read_queue(std::size_t capacity);

	~io_uring_file_read_queue() override;

	void queue(
		file_handle_t file,
		std::uint64_t offset,
		span<const memory_range> buffers,
		std::uint64_t tag
	) override;

	file_read_completion wait() override;

private:
	// A read the kernel is carrying out, which it names by the address of
	// this.
	struct request
	{
		file_handle_t file;
		std::uint64_t offset;
		std::uint64_t tag;
		std::vector<memory_range> buffers;
		std::size_t done;
		std::vector<iovec> vectors;
	};

	// Hands the kernel what is left of a read, and tells whether anything
	// was.
	bool send(request &read) noexcept;

	// Waits until the kernel completes a read.
	file_read_completion receive();

	void submit();

	std::vector<request> m_requests;
	std::vector<request*> m_free_requests;

	// The reads that completed while the queue made room for others.
	std::deque<file_read_completion> m_completions;

	io_uring m_ring;
};

} // namespace vitrio
