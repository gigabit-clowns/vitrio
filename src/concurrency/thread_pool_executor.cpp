// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/concurrency/thread_pool_executor.hpp>

#include <logger.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace vitrio
{

class thread_pool_executor::implementation
{
public:
	explicit implementation(std::size_t worker_count)
		: m_worker_count(worker_count)
		, m_stopping(false)
	{
		m_workers.reserve(worker_count);
		for (std::size_t i = 0; i < worker_count; ++i)
		{
			m_workers.emplace_back([this] { run_worker(); });
		}
	}

	~implementation()
	{
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			m_stopping = true;
		}
		m_cv.notify_all();

		for (auto &worker : m_workers)
		{
			worker.join();
		}
	}

	std::size_t get_worker_count() const noexcept
	{
		return m_worker_count;
	}

	// Whether the calling thread is one of the workers, which are only
	// started by the constructor.
	bool runs_on_this_thread() const noexcept
	{
		const auto self = std::this_thread::get_id();
		return std::any_of(
			m_workers.begin(),
			m_workers.end(),
			[self](const std::thread &worker)
			{
				return worker.get_id() == self;
			}
		);
	}

	void submit(
		std::unique_ptr<task> t,
		std::shared_ptr<completion_notifier> notifier
	)
	{
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			m_queue.emplace_back(std::move(t), std::move(notifier));
		}
		m_cv.notify_one();
	}

private:
	using entry_type = std::pair<
		std::unique_ptr<task>,
		std::shared_ptr<completion_notifier>
	>;

	std::size_t m_worker_count;
	std::vector<std::thread> m_workers;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::deque<entry_type> m_queue;
	bool m_stopping;

	void run_worker()
	{
		for (;;)
		{
			entry_type entry;
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_cv.wait(
					lock,
					[this] { return m_stopping || !m_queue.empty(); }
				);

				if (m_queue.empty())
				{
					return;
				}

				entry = std::move(m_queue.front());
				m_queue.pop_front();
			}

			run_entry(entry);
		}
	}

	static void run_entry(entry_type &entry)
	{
		std::exception_ptr error;
		try
		{
			entry.first->run();
		}
		catch (...)
		{
			error = std::current_exception();
		}

		entry.second->notify_done(error);
	}
};

thread_pool_executor::thread_pool_executor(std::size_t worker_count)
{
	if (worker_count == 0)
	{
		throw std::invalid_argument(
			"thread_pool_executor: The worker count must be greater than "
			"zero."
		);
	}

	m_implementation = std::make_unique<implementation>(worker_count);
}

// A task, or what reports it, may hold the last reference to the executor,
// which is then let go on one of its own workers. A thread can not wait for
// itself to end, so the workers are then waited for on a thread of their
// own, once the task that let the executor go has returned.
thread_pool_executor::~thread_pool_executor()
{
	if (!m_implementation->runs_on_this_thread())
	{
		return;
	}

	auto *const workers = m_implementation.release();
	try
	{
		std::thread([workers] { delete workers; }).detach();
	}
	catch (...)
	{
		VITRIO_LOG_ERROR(
			"A thread_pool_executor let go on one of its own threads could "
			"not be destroyed, and is kept."
		);
	}
}

std::size_t thread_pool_executor::get_worker_count() const noexcept
{
	return m_implementation->get_worker_count();
}

void thread_pool_executor::submit(
	std::unique_ptr<task> t,
	std::shared_ptr<completion_notifier> notifier
)
{
	if (!t)
	{
		throw std::invalid_argument(
			"thread_pool_executor: The task must not be null."
		);
	}

	if (!notifier)
	{
		throw std::invalid_argument(
			"thread_pool_executor: The notifier must not be null."
		);
	}

	m_implementation->submit(std::move(t), std::move(notifier));
}

} // namespace vitrio
