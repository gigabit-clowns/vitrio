// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_executor.hpp"
#include "file_transfer_worker.hpp"

#include <system/file_transfer_port_opener.hpp>

#include <vitrio/concurrency/executor.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

namespace vitrio
{

/**
 * @brief An executor of file transfers backed by a fixed set of threads,
 * each carrying out many operations at once, with a port of its own.
 *
 * The operations are handed to the threads in turn. Each thread waits on its
 * port for the reads of its operations and for the tasks they run, which run
 * on an executor of computations.
 *
 * Destroying it waits until every operation submitted has been reported.
 * Nothing may be submitted to it once that has begun, and it must not be
 * destroyed from one of its threads, by an operation or a notifier.
 */
class thread_pool_file_transfer_executor final
	: public file_transfer_executor
{
public:
	/**
	 * @brief Start a number of threads, each with a port of its own.
	 *
	 * @param tasks Where the tasks of the operations run. Must not be null.
	 * @param ports What opens the port of each thread.
	 * @param worker_count How many threads to start. Must not be zero.
	 * @throws std::invalid_argument If @p tasks is null or @p worker_count is
	 * zero.
	 * @throws std::system_error If a thread can not be started.
	 */
	thread_pool_file_transfer_executor(
		std::shared_ptr<executor> tasks,
		const file_transfer_port_opener &ports,
		std::size_t worker_count
	);
	~thread_pool_file_transfer_executor() override;

	void submit(
		std::unique_ptr<file_transfer_operation> operation,
		std::shared_ptr<completion_notifier> done
	) override;

private:
	std::vector<std::unique_ptr<file_transfer_worker>> m_workers;
	std::atomic<std::size_t> m_next_worker;
};

} // namespace vitrio
