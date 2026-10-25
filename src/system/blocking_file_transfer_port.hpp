// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "blocking_file_transfer_queue.hpp"
#include "file_transfer_port.hpp"

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace vitrio
{

/**
 * @brief A port that carries out each transfer as it is queued, on the
 * thread that queues it, and waits on a condition variable to be woken.
 *
 * It needs nothing of the system beyond reading and writing a file, so it
 * serves where no transfer can be left to the kernel.
 */
class blocking_file_transfer_port final
	: public file_transfer_port
{
public:
	blocking_file_transfer_port();
	~blocking_file_transfer_port() override;

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

	void wake() noexcept override;
	boost::optional<file_transfer_completion> wait() override;

private:
	blocking_file_transfer_queue m_queue;

	// The transfers whose completions wait has not returned yet.
	std::size_t m_outstanding;

	std::mutex m_mutex;
	std::condition_variable m_woken_condition;
	bool m_woken;
};

} // namespace vitrio
