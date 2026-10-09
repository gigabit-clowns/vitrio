// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <exception>

namespace vitrio
{

/**
 * @brief Where a finished unit of work is reported.
 *
 * Disjoint from @ref completion on purpose: an @ref executor is handed
 * this, never the query side, and a caller of @ref executor::submit is
 * handed the query side back, never this.
 */
class VITRIO_API completion_notifier
{
public:
	completion_notifier() = default;
	completion_notifier(const completion_notifier &other) = default;
	completion_notifier(completion_notifier &&other) = default;
	virtual ~completion_notifier() = default;

	completion_notifier&
	operator=(const completion_notifier &other) = default;
	completion_notifier&
	operator=(completion_notifier &&other) = default;

	/**
	 * @brief Report that one unit of work has finished.
	 *
	 * Called exactly once per unit of work this notifier stands for.
	 *
	 * @param error The exception the unit of work raised, or null if it
	 * succeeded.
	 *
	 * @par Thread safety
	 * May be called concurrently, from as many threads as there are units
	 * of work.
	 */
	virtual void notify_done(std::exception_ptr error = nullptr) = 0;
};

} // namespace vitrio
