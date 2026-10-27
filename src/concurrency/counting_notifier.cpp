// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/concurrency/counting_notifier.hpp>

#include <atomic>
#include <cstddef>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace vitrio
{

class counting_notifier::implementation
{
public:
	implementation(
		std::size_t count,
		std::shared_ptr<completion_notifier> parent
	)
		: m_remaining(count)
		, m_parent(std::move(parent))
	{
	}

	void notify_done(std::exception_ptr error)
	{
		if (error)
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_first_error)
			{
				m_first_error = error;
			}
		}

		if (m_remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			report();
		}
	}

	void report()
	{
		std::exception_ptr error;
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			error = m_first_error;
		}
		m_parent->notify_done(error);
	}

private:
	std::atomic<std::size_t> m_remaining;
	std::shared_ptr<completion_notifier> m_parent;
	std::mutex m_mutex;
	std::exception_ptr m_first_error;
};

counting_notifier::counting_notifier(
	std::size_t count,
	std::shared_ptr<completion_notifier> parent
)
{
	if (!parent)
	{
		throw std::invalid_argument(
			"counting_notifier: The parent notifier must not be null."
		);
	}

	m_implementation =
		std::make_unique<implementation>(count, std::move(parent));
	if (count == 0)
	{
		m_implementation->report();
	}
}

counting_notifier::~counting_notifier() = default;

void counting_notifier::notify_done(std::exception_ptr error)
{
	m_implementation->notify_done(error);
}

} // namespace vitrio
