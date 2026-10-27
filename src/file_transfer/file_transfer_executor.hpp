// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_operation.hpp"

#include <vitrio/concurrency/completion_notifier.hpp>

#include <memory>

namespace vitrio
{

/**
 * @brief Where a @ref file_transfer_operation is handed off to be carried
 * out.
 *
 * It starts the operation on a thread of its own and calls it as what it
 * queues and runs completes, as the operation says. Once the operation is
 * done, or has failed and nothing it started is outstanding, it destroys the
 * operation, then reports to the notifier it was submitted with exactly once:
 * what the call that failed it threw, or nothing.
 *
 * @par Thread safety
 * @ref submit may be called concurrently, from any thread.
 */
class file_transfer_executor
{
public:
	file_transfer_executor() noexcept;
	file_transfer_executor(const file_transfer_executor &other) = delete;
	file_transfer_executor(file_transfer_executor &&other) = delete;
	virtual ~file_transfer_executor();

	file_transfer_executor&
	operator=(const file_transfer_executor &other) = delete;
	file_transfer_executor&
	operator=(file_transfer_executor &&other) = delete;

	/**
	 * @brief Hand an operation off to be carried out.
	 *
	 * @param operation The operation. Must not be null.
	 * @param done Where its outcome is reported. Must not be null.
	 * @throws std::invalid_argument If @p operation or @p done is null.
	 */
	virtual void submit(
		std::unique_ptr<file_transfer_operation> operation,
		std::shared_ptr<completion_notifier> done
	) = 0;
};

} // namespace vitrio
