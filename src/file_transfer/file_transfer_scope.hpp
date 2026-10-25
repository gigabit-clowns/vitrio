// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <system/file_transfer_submitter.hpp>

#include <vitrio/concurrency/task.hpp>

#include <cstdint>
#include <memory>

namespace vitrio
{

/**
 * @brief What a @ref file_transfer_operation queues its transfers and runs its
 * tasks through, during one of its calls.
 *
 * A transfer it queues is outstanding until its completion reaches
 * @ref file_transfer_operation::on_transfer, and a task it runs until its
 * outcome reaches @ref file_transfer_operation::on_task, each carrying the
 * tag it was given.
 */
class file_transfer_scope
	: public file_transfer_submitter
{
public:
	file_transfer_scope() noexcept;
	~file_transfer_scope() override;

	/**
	 * @brief Run a task on the executor of computations.
	 *
	 * The task runs on another thread, so the operation does not wait for
	 * what it computes, such as values converted on their way to an array.
	 *
	 * @param work The task. Must not be null.
	 * @param tag What its outcome carries, to tell it from the others.
	 * @throws std::invalid_argument If @p work is null.
	 */
	virtual void run(std::unique_ptr<task> work, std::uint64_t tag) = 0;
};

} // namespace vitrio
