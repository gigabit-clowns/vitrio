// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_read_plan_execution.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <system/file_read_completion.hpp>
#include <system/file_read_queue.hpp>

#include <exception>
#include <system_error>

namespace vitrio
{

file_read_plan_execution::file_read_plan_execution(
	const std::string &path,
	file_handle_t file,
	const file_read_plan &plan,
	file_read_queue &queue
)
	: m_path(path)
	, m_file(file)
	, m_plan(plan)
	, m_queue(queue)
	, m_pending(plan.get_step_count(), 0)
	, m_outstanding(0)
	, m_next_read(0)
	, m_sent_steps(0)
	, m_done_steps(0)
	, m_taken(false)
	, m_taken_read(0)
{
}

file_read_plan_execution::~file_read_plan_execution()
{
	wait_out();
}

bool file_read_plan_execution::take_next(std::size_t &read)
{
	release_taken();
	send_ahead();
	if (m_done_steps == m_plan.get_step_count())
	{
		return false;
	}

	const auto completion = m_queue.wait();
	--m_outstanding;
	read = static_cast<std::size_t>(completion.get_tag());

	if (completion.get_error())
	{
		throw std::system_error(completion.get_error());
	}

	if (completion.get_size() < m_plan.get_size(read))
	{
		throw image_file_format_error(
			m_path + ": file_read_plan_execution: The file ends before a "
			"stretch it is to be read from does."
		);
	}

	m_taken = true;
	m_taken_read = read;
	return true;
}

void file_read_plan_execution::release_taken() noexcept
{
	if (m_taken)
	{
		--m_pending[m_plan.get_step(m_taken_read)];
		m_taken = false;
	}
}

// Sends the step that is not done with and the one after it, past the steps
// whose reads are all done with.
void file_read_plan_execution::send_ahead()
{
	const auto step_count = m_plan.get_step_count();
	for (;;)
	{
		while (m_sent_steps < step_count && m_sent_steps < m_done_steps + 2)
		{
			send_step(m_sent_steps++);
		}

		if (m_done_steps == step_count || m_pending[m_done_steps] > 0)
		{
			return;
		}

		++m_done_steps;
	}
}

void file_read_plan_execution::send_step(std::size_t step)
{
	for (; m_next_read < m_plan.get_read_count(); ++m_next_read)
	{
		if (m_plan.get_step(m_next_read) != step)
		{
			return;
		}

		m_queue.queue(
			m_file,
			m_plan.get_offset(m_next_read),
			m_plan.get_buffers(m_next_read),
			m_next_read
		);
		++m_outstanding;
		++m_pending[step];
	}
}

// Takes the completions of the reads still outstanding, and discards them.
void file_read_plan_execution::wait_out() noexcept
{
	while (m_outstanding > 0)
	{
		try
		{
			m_queue.wait();
			--m_outstanding;
		}
		catch (const std::system_error &error)
		{
			// The system is short of memory for the reads that are queued,
			// and may have it later. Failing otherwise, it could still write
			// into their buffers, which nothing could then release safely.
			const auto code = error.code();
			if (
				code != std::errc::resource_unavailable_try_again &&
				code != std::errc::device_or_resource_busy
			)
			{
				std::terminate();
			}
		}
	}
}

} // namespace vitrio
