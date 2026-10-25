// SPDX-License-Identifier: LGPL-2.1-or-later

#include "thread_pool_file_transfer_executor.hpp"

#include <stdexcept>
#include <utility>

namespace vitrio
{

thread_pool_file_transfer_executor::thread_pool_file_transfer_executor(
	std::shared_ptr<executor> tasks,
	const file_transfer_port_opener &ports,
	std::size_t worker_count
)
	: m_next_worker(0)
{
	if (!tasks)
	{
		throw std::invalid_argument(
			"thread_pool_file_transfer_executor: The executor of tasks must "
			"not be null."
		);
	}

	if (worker_count == 0)
	{
		throw std::invalid_argument(
			"thread_pool_file_transfer_executor: The worker count must be "
			"greater than zero."
		);
	}

	m_workers.reserve(worker_count);
	for (std::size_t index = 0; index < worker_count; ++index)
	{
		m_workers.push_back(
			std::make_unique<file_transfer_worker>(ports.open(), tasks)
		);
	}
}

thread_pool_file_transfer_executor::~thread_pool_file_transfer_executor() =
	default;

void thread_pool_file_transfer_executor::submit(
	std::unique_ptr<file_transfer_operation> operation,
	std::shared_ptr<completion_notifier> done
)
{
	if (!operation)
	{
		throw std::invalid_argument(
			"thread_pool_file_transfer_executor: The operation must not be "
			"null."
		);
	}

	if (!done)
	{
		throw std::invalid_argument(
			"thread_pool_file_transfer_executor: The notifier must not be "
			"null."
		);
	}

	const auto next = m_next_worker.fetch_add(1, std::memory_order_relaxed);
	m_workers[next % m_workers.size()]->submit(
		std::move(operation),
		std::move(done)
	);
}

} // namespace vitrio
