// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_dispatch.hpp"
#include "file_transfer_inbox.hpp"
#include "file_transfer_operation.hpp"

#include <system/file_transfer_port.hpp>

#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/concurrency/executor.hpp>

#include <boost/optional.hpp>

#include <memory>
#include <thread>

namespace vitrio
{

/**
 * @brief A thread that carries out file transfer operations, waiting on one
 * port for their transfers and for its mail.
 *
 * Destroying the worker waits until every operation submitted to it has been
 * reported, then destroys the port.
 *
 * @par Thread safety
 * @ref submit may be called concurrently, from any thread, the worker's own
 * included.
 */
class file_transfer_worker
{
public:
	/**
	 * @brief Start a thread.
	 *
	 * @param port What the thread queues the reads of its operations on, and
	 * waits on. Must not be null.
	 * @param tasks Where the tasks of its operations run. Must not be null.
	 * @throws std::system_error If the thread can not be started.
	 */
	file_transfer_worker(
		std::unique_ptr<file_transfer_port> port,
		std::shared_ptr<executor> tasks
	);

	file_transfer_worker(const file_transfer_worker &other) = delete;
	file_transfer_worker(file_transfer_worker &&other) = delete;

	/**
	 * @pre Nothing submits to the worker any more.
	 */
	~file_transfer_worker();

	file_transfer_worker&
	operator=(const file_transfer_worker &other) = delete;
	file_transfer_worker&
	operator=(file_transfer_worker &&other) = delete;

	/**
	 * @brief Hand an operation to the thread.
	 *
	 * @param operation The operation. Must not be null.
	 * @param done Where its outcome is reported. Must not be null.
	 */
	void submit(
		std::unique_ptr<file_transfer_operation> operation,
		std::shared_ptr<completion_notifier> done
	);

private:
	void run() noexcept;

	// Waits on the port until a transfer completes or the port is woken.
	// Only a shortage of the kernel is waited out: the kernel may still be
	// filling buffers of reads it could not be waited for, which nothing
	// could then release safely, so any other failure ends the process.
	boost::optional<file_transfer_completion> wait() noexcept;

	std::unique_ptr<file_transfer_port> m_port;
	std::shared_ptr<executor> m_tasks;
	std::shared_ptr<file_transfer_inbox> m_inbox;
	file_transfer_dispatch m_dispatch;
	std::thread m_thread;
};

} // namespace vitrio
