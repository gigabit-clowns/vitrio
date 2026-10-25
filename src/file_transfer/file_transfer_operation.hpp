// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_scope.hpp"

#include <system/file_transfer_completion.hpp>

#include <cstdint>
#include <exception>

namespace vitrio
{

/**
 * @brief A transfer between files and memory, carried out a step at a time
 * as the reads, writes, syncs and tasks it starts complete.
 *
 * A @ref file_transfer_executor calls it from one thread at a time: @ref start
 * once, then @ref on_transfer for each transfer it queued and @ref on_task
 * for each task it ran, as each completes, in any order. A call queues
 * transfers and runs tasks through the scope it is given, which serves during
 * the call only, and returns without waiting for any of them: the thread that
 * calls it carries out many operations at once.
 *
 * An operation is done once a call of it returns with nothing it queued or
 * ran outstanding. A call that throws fails it with what it threw: it is
 * called no more, and kept until what it queued and ran has completed, so
 * that no transfer outlives the memory it moves.
 */
class file_transfer_operation
{
public:
	file_transfer_operation() noexcept;
	file_transfer_operation(const file_transfer_operation &other) = delete;
	file_transfer_operation(file_transfer_operation &&other) = delete;
	virtual ~file_transfer_operation();

	file_transfer_operation&
	operator=(const file_transfer_operation &other) = delete;
	file_transfer_operation&
	operator=(file_transfer_operation &&other) = delete;

	/**
	 * @brief Start the transfer.
	 *
	 * @param scope What the transfers and tasks of the call go through.
	 */
	virtual void start(file_transfer_scope &scope) = 0;

	/**
	 * @brief Carry on from a transfer that completed.
	 *
	 * @param completion How the transfer ended, with the tag it was queued
	 * with.
	 * @param scope What the transfers and tasks of the call go through.
	 */
	virtual void on_transfer(
		const file_transfer_completion &completion,
		file_transfer_scope &scope
	) = 0;

	/**
	 * @brief Carry on from a task that finished.
	 *
	 * @param tag The tag the task was run with.
	 * @param error What the task threw, or null if it succeeded.
	 * @param scope What the transfers and tasks of the call go through.
	 */
	virtual void on_task(
		std::uint64_t tag,
		std::exception_ptr error,
		file_transfer_scope &scope
	) = 0;
};

} // namespace vitrio
