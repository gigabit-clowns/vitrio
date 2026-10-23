// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/concurrency/completion_notifier.hpp>

#include <exception>
#include <memory>

namespace vitrio
{

/**
 * @brief A notifier that reports failures to another, and nothing else.
 *
 * For a unit of work that hands its outcome on to work it starts: should it
 * fail before it starts that work, the failure is its outcome; should it
 * succeed, the work it started reports instead. A task that opens a file and
 * starts reading it is submitted with one, and passes the notifier behind it
 * to the read.
 *
 * @par Thread safety
 * @ref notify_done may be called concurrently.
 */
class failure_forwarding_notifier final
	: public completion_notifier
{
public:
	/**
	 * @brief Construct a notifier forwarding failures to another.
	 *
	 * @param parent Where a failure is reported. Must not be null.
	 * @throws std::invalid_argument If @p parent is null.
	 */
	explicit failure_forwarding_notifier(
		std::shared_ptr<completion_notifier> parent
	);
	~failure_forwarding_notifier() override;

	failure_forwarding_notifier(
		const failure_forwarding_notifier &other
	) = delete;
	failure_forwarding_notifier(failure_forwarding_notifier &&other) = delete;
	failure_forwarding_notifier&
	operator=(const failure_forwarding_notifier &other) = delete;
	failure_forwarding_notifier&
	operator=(failure_forwarding_notifier &&other) = delete;

	/**
	 * @brief Report a failure to the parent notifier, or nothing if the
	 * unit of work succeeded.
	 *
	 * @param error The exception the unit of work raised, or null if it
	 * succeeded.
	 */
	void notify_done(std::exception_ptr error = nullptr) override;

private:
	std::shared_ptr<completion_notifier> m_parent;
};

} // namespace vitrio
