// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_transfer_task_notifier.hpp"

#include <assert.hpp>

#include <utility>

namespace vitrio
{

file_transfer_task_notifier::file_transfer_task_notifier(
	std::shared_ptr<file_transfer_inbox> inbox,
	std::uint64_t route
) noexcept
	: m_inbox(std::move(inbox))
	, m_route(route)
{
	VITRIO_ASSERT(m_inbox);
}

file_transfer_task_notifier::~file_transfer_task_notifier() = default;

void file_transfer_task_notifier::notify_done(std::exception_ptr error)
{
	m_inbox->post(file_transfer_inbox::task_outcome{m_route, std::move(error)});
}

} // namespace vitrio
