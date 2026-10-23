// SPDX-License-Identifier: LGPL-2.1-or-later

#include "thread_pool_file_transfer_executor.hpp"

#include <stdexcept>
#include <utility>

namespace vitrio
{

thread_pool_file_transfer_executor::thread_pool_file_transfer_executor(
	std::shared_ptr<executor> tasks,
	std::vector<std::unique_ptr<file_read_port>> ports
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

	if (ports.empty())
	{
		throw std::invalid_argument(
			"thread_pool_file_transfer_executor: There must be a port."
		);
	}

	for (const auto &port : ports)
	{
		if (!port)
		{
			throw std::invalid_argument(
				"thread_pool_file_transfer_executor: A port must not be null."
			);
		}
	}

	m_workers.reserve(ports.size());
	for (auto &port : ports)
	{
		m_workers.push_back(
			std::make_unique<file_transfer_worker>(std::move(port), tasks)
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
