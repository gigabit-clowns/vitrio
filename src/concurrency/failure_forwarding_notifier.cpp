// SPDX-License-Identifier: LGPL-2.1-or-later

#include "failure_forwarding_notifier.hpp"

#include <stdexcept>
#include <utility>

namespace vitrio
{

failure_forwarding_notifier::failure_forwarding_notifier(
	std::shared_ptr<completion_notifier> parent
)
	: m_parent(std::move(parent))
{
	if (!m_parent)
	{
		throw std::invalid_argument(
			"failure_forwarding_notifier: The parent notifier must not be "
			"null."
		);
	}
}

failure_forwarding_notifier::~failure_forwarding_notifier() = default;

void failure_forwarding_notifier::notify_done(std::exception_ptr error)
{
	if (error)
	{
		m_parent->notify_done(error);
	}
}

} // namespace vitrio
