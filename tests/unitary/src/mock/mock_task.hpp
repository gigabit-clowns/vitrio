// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/concurrency/task.hpp>

#include <trompeloeil.hpp>

#include <memory>
#include <vector>

namespace vitrio
{

class mock_task final
	: public task
{
public:
	mock_task() = default;

	MAKE_MOCK0(run, void(), override);

	// The executor destroys the task once it has run, so its expectations
	// go with it, and are checked then.
	std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
};

} // namespace vitrio
