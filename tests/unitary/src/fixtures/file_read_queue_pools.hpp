// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <system/asynchronous_file_read_queue_pool.hpp>
#include <system/blocking_file_read_queue.hpp>
#include <system/file_read_queue_pool.hpp>

#include <memory>
#include <vector>

namespace vitrio
{

/**
 * @brief A pool whose queues read as they are asked, which every system has.
 */
class blocking_file_read_queue_pool final
	: public file_read_queue_pool
{
public:
	file_read_queue& get_queue() override
	{
		thread_local blocking_file_read_queue queue;
		return queue;
	}
};

/**
 * @brief Every pool the process can have: a blocking one, and an
 * asynchronous one where it can have that.
 */
inline std::vector<std::shared_ptr<file_read_queue_pool>>
make_every_file_read_queue_pool()
{
	std::vector<std::shared_ptr<file_read_queue_pool>> pools = {
		std::make_shared<blocking_file_read_queue_pool>(),
	};
	if (asynchronous_file_read_queue_pool::is_supported())
	{
		pools.push_back(std::make_shared<asynchronous_file_read_queue_pool>());
	}

	return pools;
}

} // namespace vitrio
