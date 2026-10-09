// SPDX-License-Identifier: LGPL-2.1-or-later

#include "memory_loan_completion.hpp"

#include <stdexcept>
#include <utility>

namespace vitrio
{

memory_loan_completion::memory_loan_completion(
	std::shared_ptr<completion> work,
	memory_loan loan
)
	: m_loan(std::move(loan))
	, m_work(std::move(work))
{
	if (!m_work)
	{
		throw std::invalid_argument(
			"memory_loan_completion: The completion of the work is null."
		);
	}
}

memory_loan_completion::~memory_loan_completion() = default;

void memory_loan_completion::wait() const
{
	m_work->wait();
}

bool memory_loan_completion::is_ready() const noexcept
{
	return m_work->is_ready();
}

void memory_loan_completion::get() const
{
	m_work->get();
}

} // namespace vitrio
