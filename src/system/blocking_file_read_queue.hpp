// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_queue.hpp"

#include <deque>

namespace vitrio
{

/**
 * @brief A queue that carries out each read as it is queued, on the thread
 * that queues it.
 */
class blocking_file_read_queue final
	: public file_read_queue
{
public:
	blocking_file_read_queue();
	~blocking_file_read_queue() override;

	void queue(
		file_handle_t file,
		std::uint64_t offset,
		span<const memory_range> buffers,
		std::uint64_t tag
	) override;

	file_read_completion wait() override;

private:
	// The completions of the outstanding reads, in the order they were
	// queued.
	std::deque<file_read_completion> m_completions;
};

} // namespace vitrio
