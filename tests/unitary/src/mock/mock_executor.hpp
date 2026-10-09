// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/concurrency/executor.hpp>

#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/concurrency/task.hpp>

#include <memory>
#include <trompeloeil.hpp>

namespace vitrio
{

class mock_executor final
	: public executor
{
public:
	mock_executor() = default;

	MAKE_MOCK2(
		submit,
		void(
			std::unique_ptr<task> t,
			std::shared_ptr<completion_notifier> notifier
		),
		override
	);
};

} // namespace vitrio
