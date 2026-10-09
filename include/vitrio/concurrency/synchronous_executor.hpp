// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/concurrency/executor.hpp>
#include <vitrio/concurrency/task.hpp>
#include <vitrio/export.hpp>

#include <memory>

namespace vitrio
{

/**
 * @brief An executor that runs a task on the calling thread, before
 * submit returns.
 *
 * Spawns no thread and keeps no state. A completion fed by a notifier
 * submitted here is therefore already resolved by the time submit is
 * back.
 */
class VITRIO_API synchronous_executor final
	: public executor
{
public:
	synchronous_executor() noexcept;

	synchronous_executor(const synchronous_executor &other) = delete;
	synchronous_executor(synchronous_executor &&other) = delete;

	~synchronous_executor() override;

	synchronous_executor&
	operator=(const synchronous_executor &other) = delete;
	synchronous_executor&
	operator=(synchronous_executor &&other) = delete;

	/**
	 * @brief Run a unit of work immediately, on the calling thread.
	 *
	 * @param t The work to run. Must not be null.
	 * @param notifier Where the outcome is reported. Must not be null.
	 * @throws std::invalid_argument If @p t or @p notifier is null.
	 */
	void submit(
		std::unique_ptr<task> t,
		std::shared_ptr<completion_notifier> notifier
	) override;
};

} // namespace vitrio
