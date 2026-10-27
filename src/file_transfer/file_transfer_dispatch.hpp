// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_inbox.hpp"
#include "file_transfer_operation.hpp"

#include <system/file_transfer_completion.hpp>
#include <system/file_transfer_submitter.hpp>

#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/concurrency/executor.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <vector>

namespace vitrio
{

/**
 * @brief The operations one thread carries out: it starts them, hands each
 * the completions of what it queued and ran, and reports each once it is
 * done, as @ref file_transfer_executor says.
 *
 * Each transfer and task of an operation goes under a route of its own, the
 * tag the thread knows it by, which leads back to the operation and to the
 * tag the operation gave it. Transfers are queued through a submitter, whose
 * completions the thread hands back with @ref deliver_transfer. Tasks run on an
 * executor and report through a @ref file_transfer_task_notifier to the inbox
 * of the thread, which hands their outcomes back with @ref deliver_task.
 *
 * Whatever an operation throws is caught here, and so is whatever a notifier
 * throws, which is logged.
 *
 * @par Thread safety
 * Called from one thread at a time.
 */
class file_transfer_dispatch
{
public:
	/**
	 * @brief Construct a dispatch with no operation.
	 *
	 * @param transfers Where the transfers of the operations are queued.
	 * @param tasks Where the tasks of the operations run.
	 * @param inbox Where the outcomes of the tasks are posted. Must not be
	 * null.
	 */
	file_transfer_dispatch(
		file_transfer_submitter &transfers,
		executor &tasks,
		std::shared_ptr<file_transfer_inbox> inbox
	) noexcept;

	file_transfer_dispatch(const file_transfer_dispatch &other) = delete;
	file_transfer_dispatch(file_transfer_dispatch &&other) = delete;

	/**
	 * @pre Every operation has been reported.
	 */
	~file_transfer_dispatch();

	file_transfer_dispatch&
	operator=(const file_transfer_dispatch &other) = delete;
	file_transfer_dispatch&
	operator=(file_transfer_dispatch &&other) = delete;

	/**
	 * @brief Start an operation.
	 *
	 * @param operation The operation. Must not be null.
	 * @param done Where its outcome is reported. Must not be null.
	 */
	void admit(
		std::unique_ptr<file_transfer_operation> operation,
		std::shared_ptr<completion_notifier> done
	) noexcept;

	/**
	 * @brief Hand the completion of a transfer to the operation that queued
	 * it.
	 *
	 * @param completion The completion, tagged with the route of the
	 * transfer.
	 */
	void deliver_transfer(
		const file_transfer_completion &completion
	) noexcept;

	/**
	 * @brief Hand the outcome of a task to the operation that ran it.
	 *
	 * @param route The route of the task.
	 * @param error What the task threw, or null if it succeeded.
	 */
	void deliver_task(std::uint64_t route, std::exception_ptr error) noexcept;

	/**
	 * @brief Tell whether every operation admitted has been reported.
	 *
	 * @return bool Whether none is left.
	 */
	bool is_idle() const noexcept;

private:
	class operation_scope;

	// An operation, until it is reported.
	struct slot
	{
		std::unique_ptr<file_transfer_operation> operation;
		std::shared_ptr<completion_notifier> done;

		// Its transfers and tasks that have not been handed back.
		std::size_t outstanding;

		// What failed it, if anything did.
		std::exception_ptr failure;
	};

	// Where a transfer or a task leads back to.
	struct route
	{
		std::size_t slot;
		std::uint64_t tag;
	};

	std::size_t take_slot();

	std::uint64_t open_route(std::size_t slot, std::uint64_t tag);

	// Frees a route, and returns where it led.
	route close_route(std::uint64_t index) noexcept;

	// Calls the operation of a slot, unless it has failed, and fails it with
	// what the call throws.
	template <typename Call>
	void call(std::size_t slot, Call &&invoke) noexcept;

	// Reports the operation of a slot and frees the slot, if nothing of the
	// operation is outstanding.
	void settle(std::size_t slot) noexcept;

	file_transfer_submitter &m_transfers;
	executor &m_tasks;
	std::shared_ptr<file_transfer_inbox> m_inbox;

	std::vector<slot> m_slots;
	std::vector<std::size_t> m_free_slots;

	std::vector<route> m_routes;
	std::vector<std::uint64_t> m_free_routes;
};

} // namespace vitrio
