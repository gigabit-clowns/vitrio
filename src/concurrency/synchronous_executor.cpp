// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/concurrency/synchronous_executor.hpp>

#include <exception>
#include <stdexcept>
#include <utility>

namespace vitrio
{

synchronous_executor::synchronous_executor() noexcept = default;

synchronous_executor::~synchronous_executor() = default;

void synchronous_executor::submit(
	std::unique_ptr<task> t,
	std::shared_ptr<completion_notifier> notifier
)
{
	if (!t)
	{
		throw std::invalid_argument(
			"synchronous_executor: The task must not be null."
		);
	}

	if (!notifier)
	{
		throw std::invalid_argument(
			"synchronous_executor: The notifier must not be null."
		);
	}

	std::exception_ptr error;
	try
	{
		t->run();
	}
	catch (...)
	{
		error = std::current_exception();
	}

	notifier->notify_done(error);
}

} // namespace vitrio
