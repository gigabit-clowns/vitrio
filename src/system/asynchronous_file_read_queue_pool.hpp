// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_queue_pool.hpp"

namespace vitrio
{

/**
 * @brief A pool of queues whose reads the system carries out while their
 * thread goes on.
 *
 * A thread gets a queue that reads as it is asked instead where the system
 * can not open an asynchronous one for it. The queues belong to the threads,
 * so every pool gives a thread the same one.
 */
class asynchronous_file_read_queue_pool final
	: public file_read_queue_pool
{
public:
	/**
	 * @brief Construct a pool.
	 *
	 * @pre The process can have asynchronous queues: @ref is_supported.
	 */
	asynchronous_file_read_queue_pool() noexcept;

	~asynchronous_file_read_queue_pool() override;

	/**
	 * @brief Tell whether the process can have asynchronous queues of file
	 * reads.
	 *
	 * @return bool Whether it can. The same for as long as the process
	 * lives.
	 */
	static bool is_supported() noexcept;

	file_read_queue& get_queue() override;
};

} // namespace vitrio
