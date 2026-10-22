// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_queue.hpp"

namespace vitrio
{

/**
 * @brief Queues of file reads kept for reuse, one for each thread.
 */
class file_read_queue_pool
{
public:
	file_read_queue_pool() noexcept;
	file_read_queue_pool(const file_read_queue_pool &other) = delete;
	file_read_queue_pool(file_read_queue_pool &&other) = delete;
	virtual ~file_read_queue_pool();

	file_read_queue_pool&
	operator=(const file_read_queue_pool &other) = delete;
	file_read_queue_pool& operator=(file_read_queue_pool &&other) = delete;

	/**
	 * @brief Get the queue of the calling thread.
	 *
	 * @return file_read_queue& The queue. Every call from one thread gets
	 * the same one, no other thread gets it, and it lives as long as the
	 * thread.
	 * @throws std::bad_alloc If the queue can not be made.
	 */
	virtual file_read_queue& get_queue() = 0;
};

} // namespace vitrio
