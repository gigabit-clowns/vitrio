// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

namespace vitrio
{

/**
 * @brief One unit of work an @ref executor can run.
 *
 * @ref run is expected not to let an exception escape when submitted
 * alongside a @ref completion_notifier: whichever @ref executor runs it
 * catches what it throws and reports it through that notifier instead.
 */
class VITRIO_API task
{
public:
	task() = default;
	task(const task &other) = default;
	task(task &&other) = default;
	virtual ~task() = default;

	task& operator=(const task &other) = default;
	task& operator=(task &&other) = default;

	/**
	 * @brief Run this unit of work.
	 */
	virtual void run() = 0;
};

} // namespace vitrio
