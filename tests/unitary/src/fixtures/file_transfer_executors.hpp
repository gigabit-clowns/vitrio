// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <file_transfer/file_transfer_executor.hpp>
#include <file_transfer/file_transfer_operation.hpp>
#include <file_transfer/thread_pool_file_transfer_executor.hpp>
#include <system/asynchronous_file_transfer_port_opener.hpp>
#include <system/blocking_file_transfer_port_opener.hpp>

#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/concurrency/thread_pool_executor.hpp>

#include <memory>
#include <vector>

namespace vitrio
{

/**
 * @brief Every executor of file transfers the process can have, each running
 * its tasks on a thread pool of its own: one whose ports read as they are
 * asked, and one whose ports are asynchronous where the process can have
 * them.
 */
inline std::vector<std::shared_ptr<file_transfer_executor>>
make_every_file_transfer_executor()
{
	std::vector<std::shared_ptr<file_transfer_executor>> executors = {
		std::make_shared<thread_pool_file_transfer_executor>(
			std::make_shared<thread_pool_executor>(2),
			blocking_file_transfer_port_opener(),
			1
		),
	};

	// Ports with room for few reads, so that reads queue past what they
	// hold.
	if (asynchronous_file_transfer_port_opener::is_supported())
	{
		executors.push_back(
			std::make_shared<thread_pool_file_transfer_executor>(
				std::make_shared<thread_pool_executor>(2),
				asynchronous_file_transfer_port_opener(4),
				1
			)
		);
	}

	return executors;
}

/**
 * @brief Carry out an operation and wait for it.
 *
 * @throws Whatever the operation reports.
 */
inline void run_now(
	file_transfer_executor &executor,
	std::unique_ptr<file_transfer_operation> operation
)
{
	const auto done = std::make_shared<counting_completion>(1);
	executor.submit(std::move(operation), done);
	done->get();
}

} // namespace vitrio
