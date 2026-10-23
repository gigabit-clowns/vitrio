// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/concurrency/completion_notifier.hpp>

#include <trompeloeil.hpp>

#include <exception>

namespace vitrio
{

class mock_completion_notifier final
	: public completion_notifier
{
public:
	mock_completion_notifier() = default;

	MAKE_MOCK1(notify_done, void(std::exception_ptr error), override);
};

} // namespace vitrio
