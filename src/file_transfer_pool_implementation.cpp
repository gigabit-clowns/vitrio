// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_transfer_pool_implementation.hpp"

#include <file_transfer/thread_pool_file_transfer_executor.hpp>
#include <system/asynchronous_file_transfer_port_opener.hpp>
#include <system/blocking_file_transfer_port_opener.hpp>
#include <system/file_transfer_port_opener.hpp>

#include <assert.hpp>
#include <logger.hpp>
#include <tuning.hpp>

#include <memory>
#include <thread>
#include <utility>

namespace vitrio
{

namespace
{

// What opens the ports of the threads: one of asynchronous ports where the
// process can have them, and of ports that read as they are asked otherwise.
std::unique_ptr<file_transfer_port_opener> make_port_opener()
{
	if (asynchronous_file_transfer_port_opener::is_supported())
	{
		return std::make_unique<asynchronous_file_transfer_port_opener>(
			get_file_transfer_queue_capacity()
		);
	}

	return std::make_unique<blocking_file_transfer_port_opener>();
}

// Destroys an executor, which waits for its threads. Whatever holds the
// executor may be let go by a transfer of its own, on one of those threads,
// which can not wait for itself, so the executor is then destroyed on a
// thread of its own instead.
void destroy(thread_pool_file_transfer_executor *executor) noexcept
{
	if (executor->runs_on_this_thread())
	{
		try
		{
			std::thread([executor] { delete executor; }).detach();
		}
		catch (...)
		{
			// Short of a thread, the executor is kept rather than the
			// process ended.
			VITRIO_LOG_ERROR(
				"A pool of file transfers let go on one of its own threads "
				"could not be destroyed, and is kept."
			);
		}
		return;
	}

	delete executor;
}

std::shared_ptr<file_transfer_executor> make_executor(
	std::shared_ptr<executor> tasks
)
{
	auto made = std::make_unique<thread_pool_file_transfer_executor>(
		std::move(tasks),
		*make_port_opener(),
		get_file_transfer_workers()
	);
	return std::shared_ptr<thread_pool_file_transfer_executor>(
		made.release(),
		&destroy
	);
}

} // anonymous namespace

file_transfer_pool::implementation::implementation(
	std::shared_ptr<executor> tasks
)
	: m_executor(make_executor(std::move(tasks)))
{
}

file_transfer_pool::implementation::~implementation() = default;

const std::shared_ptr<file_transfer_executor>&
file_transfer_pool::implementation::get_executor() const noexcept
{
	return m_executor;
}

} // namespace vitrio
