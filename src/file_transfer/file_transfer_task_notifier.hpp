// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_inbox.hpp"

#include <vitrio/concurrency/completion_notifier.hpp>

#include <cstdint>
#include <memory>

namespace vitrio
{

/**
 * @brief A notifier that posts the outcome of a task an operation ran to the
 * inbox of the thread that carries out the operation.
 *
 * It keeps the inbox alive, since the executor that ran the task may keep the
 * notifier after the thread is gone.
 *
 * @par Thread safety
 * @ref notify_done may be called from any thread.
 */
class file_transfer_task_notifier final
	: public completion_notifier
{
public:
	/**
	 * @brief Construct a notifier for one task.
	 *
	 * @param inbox Where the outcome is posted. Must not be null.
	 * @param route The route the thread knows the task by.
	 */
	file_transfer_task_notifier(
		std::shared_ptr<file_transfer_inbox> inbox,
		std::uint64_t route
	) noexcept;
	~file_transfer_task_notifier() override;

	file_transfer_task_notifier(
		const file_transfer_task_notifier &other
	) = delete;
	file_transfer_task_notifier(file_transfer_task_notifier &&other) = delete;
	file_transfer_task_notifier&
	operator=(const file_transfer_task_notifier &other) = delete;
	file_transfer_task_notifier&
	operator=(file_transfer_task_notifier &&other) = delete;

	void notify_done(std::exception_ptr error = nullptr) override;

private:
	std::shared_ptr<file_transfer_inbox> m_inbox;
	std::uint64_t m_route;
};

} // namespace vitrio
