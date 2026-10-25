// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_read_plan_execution.hpp"

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>

#include <assert.hpp>

namespace vitrio
{

file_read_plan_execution::file_read_plan_execution(
	const std::string &path,
	file_handle_t file,
	const file_read_plan &plan
)
	: m_path(path)
	, m_file(file)
	, m_plan(plan)
	, m_pending(plan.get_step_count(), 0)
	, m_next_read(0)
	, m_sent_steps(0)
	, m_done_steps(0)
{
}

file_read_plan_execution::~file_read_plan_execution() = default;

void file_read_plan_execution::start(file_transfer_submitter &reads)
{
	send_ahead(reads);
}

std::size_t file_read_plan_execution::take(
	const file_transfer_completion &completion
) const
{
	const auto read = static_cast<std::size_t>(completion.get_tag());
	VITRIO_ASSERT(read < m_plan.get_read_count());

	if (completion.get_error())
	{
		throw image_file_error(
			m_path + ": file_read_plan_execution: The file could not be "
			"read: " + completion.get_error().message()
		);
	}

	if (completion.get_size() < m_plan.get_size(read))
	{
		throw image_file_format_error(
			m_path + ": file_read_plan_execution: The file ends before a "
			"stretch it is to be read from does."
		);
	}

	return read;
}

void file_read_plan_execution::release(
	std::size_t read,
	file_transfer_submitter &reads
)
{
	const auto step = m_plan.get_step(read);
	VITRIO_ASSERT(m_pending[step] > 0);

	--m_pending[step];
	send_ahead(reads);
}

// Sends the step that is not done with and the one after it, past the steps
// whose reads are all done with.
void file_read_plan_execution::send_ahead(file_transfer_submitter &reads)
{
	const auto step_count = m_plan.get_step_count();
	for (;;)
	{
		while (m_sent_steps < step_count && m_sent_steps < m_done_steps + 2)
		{
			send_step(m_sent_steps++, reads);
		}

		if (m_done_steps == step_count || m_pending[m_done_steps] > 0)
		{
			return;
		}

		++m_done_steps;
	}
}

void file_read_plan_execution::send_step(
	std::size_t step,
	file_transfer_submitter &reads
)
{
	for (; m_next_read < m_plan.get_read_count(); ++m_next_read)
	{
		if (m_plan.get_step(m_next_read) != step)
		{
			return;
		}

		reads.queue_read(
			m_file,
			m_plan.get_offset(m_next_read),
			m_plan.get_buffers(m_next_read),
			m_next_read
		);
		++m_pending[step];
	}
}

} // namespace vitrio
