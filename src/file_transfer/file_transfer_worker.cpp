// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_transfer_worker.hpp"

#include <assert.hpp>
#include <logger.hpp>

#include <exception>
#include <system_error>
#include <utility>

namespace vitrio
{

file_transfer_worker::file_transfer_worker(
	std::unique_ptr<file_transfer_port> port,
	std::shared_ptr<executor> tasks
)
	: m_port(std::move(port))
	, m_tasks(std::move(tasks))
	, m_inbox(std::make_shared<file_transfer_inbox>(*m_port))
	, m_dispatch(*m_port, *m_tasks, m_inbox)
	, m_thread([this] { run(); })
{
}

file_transfer_worker::~file_transfer_worker()
{
	m_inbox->close();
	m_thread.join();
}

void file_transfer_worker::submit(
	std::unique_ptr<file_transfer_operation> operation,
	std::shared_ptr<completion_notifier> done
)
{
	VITRIO_ASSERT(operation);
	VITRIO_ASSERT(done);

	m_inbox->post(
		file_transfer_inbox::admission{std::move(operation), std::move(done)}
	);
}

void file_transfer_worker::run() noexcept
{
	for (;;)
	{
		auto mail = m_inbox->take();
		for (auto &outcome : mail.task_outcomes)
		{
			m_dispatch.deliver_task(outcome.route, std::move(outcome.error));
		}
		for (auto &admission : mail.admissions)
		{
			m_dispatch.admit(
				std::move(admission.operation),
				std::move(admission.done)
			);
		}

		if (mail.closed && m_dispatch.is_idle())
		{
			// The wake of the closing may have been answered already, so the
			// port is not waited on: mail that came meanwhile is taken first.
			if (mail.is_empty())
			{
				return;
			}
			continue;
		}

		const auto completion = wait();
		if (completion)
		{
			m_dispatch.deliver_transfer(*completion);
		}
	}
}

boost::optional<file_transfer_completion> file_transfer_worker::wait() noexcept
{
	for (;;)
	{
		try
		{
			return m_port->wait();
		}
		catch (const std::system_error &error)
		{
			const auto code = error.code();
			if (
				code != std::errc::resource_unavailable_try_again &&
				code != std::errc::device_or_resource_busy
			)
			{
				VITRIO_LOG_CRITICAL(
					"A thread that transfers files could not wait for its "
					"transfers ({}), which the kernel may still be filling.",
					error.what()
				);
				std::terminate();
			}
			std::this_thread::yield();
		}
		catch (const std::exception &error)
		{
			VITRIO_LOG_CRITICAL(
				"A thread that transfers files could not wait for its "
				"transfers ({}), which the kernel may still be filling.",
				error.what()
			);
			std::terminate();
		}
	}
}

} // namespace vitrio
