// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_transfer_inbox.hpp"

#include <assert.hpp>

#include <utility>

namespace vitrio
{

bool file_transfer_inbox::mail::is_empty() const noexcept
{
	return admissions.empty() && task_outcomes.empty();
}

file_transfer_inbox::file_transfer_inbox(file_read_port &port) noexcept
	: m_port(port)
	, m_closed(false)
{
}

file_transfer_inbox::~file_transfer_inbox() = default;

void file_transfer_inbox::post(admission submitted)
{
	const std::lock_guard<std::mutex> lock(m_mutex);
	VITRIO_ASSERT(!m_closed);

	const auto was_empty = is_empty();
	m_admissions.push_back(std::move(submitted));
	wake_for_post(was_empty);
}

void file_transfer_inbox::post(task_outcome outcome)
{
	const std::lock_guard<std::mutex> lock(m_mutex);

	const auto was_empty = is_empty();
	m_task_outcomes.push_back(std::move(outcome));
	wake_for_post(was_empty);
}

void file_transfer_inbox::close() noexcept
{
	const std::lock_guard<std::mutex> lock(m_mutex);
	m_closed = true;
	m_port.wake();
}

file_transfer_inbox::mail file_transfer_inbox::take() noexcept
{
	mail taken;

	const std::lock_guard<std::mutex> lock(m_mutex);
	taken.admissions.swap(m_admissions);
	taken.task_outcomes.swap(m_task_outcomes);
	taken.closed = m_closed;
	return taken;
}

void file_transfer_inbox::wake_for_post(bool was_empty) noexcept
{
	if (was_empty)
	{
		m_port.wake();
	}
}

bool file_transfer_inbox::is_empty() const noexcept
{
	return m_admissions.empty() && m_task_outcomes.empty();
}

} // namespace vitrio
