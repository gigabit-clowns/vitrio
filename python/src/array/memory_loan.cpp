// SPDX-License-Identifier: LGPL-2.1-or-later

#include "memory_loan.hpp"

#include <nanobind/nanobind.h>

#include <utility>

namespace vitrio
{

// The promise is never fulfilled. Destroying it is what makes the future
// ready, and it is destroyed with the last array that holds it.
memory_loan::memory_loan(const_host_ndarray memory)
	: m_memory(std::move(memory))
	, m_owner(std::make_shared<std::promise<void>>())
	, m_returned(m_owner->get_future())
{
}

memory_loan::memory_loan(memory_loan &&other) noexcept = default;

memory_loan::~memory_loan()
{
	if (!m_owner)
	{
		return;
	}

	m_owner.reset();

	const nanobind::gil_scoped_release release;
	m_returned.wait();
}

std::shared_ptr<void> memory_loan::get_owner() const noexcept
{
	return m_owner;
}

} // namespace vitrio
