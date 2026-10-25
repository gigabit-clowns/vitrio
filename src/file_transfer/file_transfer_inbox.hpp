// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_operation.hpp"

#include <system/file_transfer_port.hpp>

#include <vitrio/concurrency/completion_notifier.hpp>

#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>

namespace vitrio
{

/**
 * @brief The mail of a thread that carries out file transfers, which other
 * threads post to it: the operations submitted to it, and the outcomes of
 * the tasks its operations ran.
 *
 * Posting to an empty inbox wakes the port the thread waits on. The thread
 * takes all of its mail at once, so a wake is under way whenever there is
 * mail.
 *
 * @par Thread safety
 * Every member may be called concurrently.
 */
class file_transfer_inbox
{
public:
	/**
	 * @brief An operation submitted, and where its outcome is reported.
	 */
	struct admission
	{
		std::unique_ptr<file_transfer_operation> operation;
		std::shared_ptr<completion_notifier> done;
	};

	/**
	 * @brief How a task ended, and the route of the thread it goes back
	 * along.
	 */
	struct task_outcome
	{
		std::uint64_t route;
		std::exception_ptr error;
	};

	/**
	 * @brief What the thread takes at once.
	 */
	struct mail
	{
		std::vector<admission> admissions;
		std::vector<task_outcome> task_outcomes;

		// Whether the inbox was closed, and no admission will come after
		// these.
		bool closed = false;

		bool is_empty() const noexcept;
	};

	/**
	 * @brief Construct an empty inbox.
	 *
	 * @param port The port the thread waits on. It must outlive every post
	 * and close.
	 */
	explicit file_transfer_inbox(file_transfer_port &port) noexcept;

	file_transfer_inbox(const file_transfer_inbox &other) = delete;
	file_transfer_inbox(file_transfer_inbox &&other) = delete;
	~file_transfer_inbox();

	file_transfer_inbox&
	operator=(const file_transfer_inbox &other) = delete;
	file_transfer_inbox& operator=(file_transfer_inbox &&other) = delete;

	/**
	 * @brief Post an operation submitted to the thread.
	 *
	 * @param submitted The operation and its notifier.
	 * @pre The inbox is not closed.
	 */
	void post(admission submitted);

	/**
	 * @brief Post the outcome of a task an operation of the thread ran.
	 *
	 * @param outcome How the task ended, and its route.
	 */
	void post(task_outcome outcome);

	/**
	 * @brief Tell the thread that no operation will be submitted to it any
	 * more, and wake it.
	 */
	void close() noexcept;

	/**
	 * @brief Take all of the mail posted so far.
	 *
	 * @return mail The mail, which the inbox no longer holds.
	 */
	mail take() noexcept;

private:
	// Wakes the port for mail just posted, if the inbox was empty before it.
	// Called under the lock: the thread takes the mail under the lock too,
	// so it can not have finished and destroyed the port meanwhile.
	void wake_for_post(bool was_empty) noexcept;

	bool is_empty() const noexcept;

	file_transfer_port &m_port;

	std::mutex m_mutex;
	std::vector<admission> m_admissions;
	std::vector<task_outcome> m_task_outcomes;
	bool m_closed;
};

} // namespace vitrio
