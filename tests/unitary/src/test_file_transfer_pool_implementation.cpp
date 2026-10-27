// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <file_transfer_pool_implementation.hpp>

#include "mock/mock_file_transfer_operation.hpp"

#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/concurrency/executor.hpp>
#include <vitrio/concurrency/thread_pool_executor.hpp>

#include <chrono>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <utility>

#include <trompeloeil.hpp>

using namespace vitrio;

TEST_CASE(
	"a file_transfer_pool::implementation keeps its executor for whoever "
	"copies it",
	"[file_transfer_pool]"
)
{
	std::weak_ptr<file_transfer_executor> watched;
	std::shared_ptr<file_transfer_executor> kept;
	{
		const file_transfer_pool::implementation pool(
			std::make_shared<thread_pool_executor>(1)
		);
		kept = pool.get_executor();
		watched = kept;
	}

	CHECK_FALSE( watched.expired() );
	kept.reset();
	CHECK( watched.expired() );
}

namespace
{

// What lets a thread go on once another says so.
class gate
{
public:
	void open()
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_open = true;
		m_opened.notify_all();
	}

	void wait()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_opened.wait(lock, [this] { return m_open; });
	}

	// Whether it opened within a time.
	bool wait_for(std::chrono::seconds time)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_opened.wait_for(lock, time, [this] { return m_open; });
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_opened;
	bool m_open = false;
};

// A notifier that holds what it is given, and lets it go when it is.
class holding_notifier final
	: public completion_notifier
{
public:
	explicit holding_notifier(std::shared_ptr<void> held) noexcept
		: m_held(std::move(held))
	{
	}

	void notify_done(std::exception_ptr) override
	{
	}

private:
	std::shared_ptr<void> m_held;
};

// An executor of computations that opens a gate once it, and the threads
// it runs on, are gone.
class watched_executor final
	: public executor
{
public:
	explicit watched_executor(gate &destroyed)
		: m_tasks(std::make_unique<thread_pool_executor>(1))
		, m_destroyed(destroyed)
	{
	}

	~watched_executor() override
	{
		m_tasks.reset();
		m_destroyed.open();
	}

	void submit(
		std::unique_ptr<task> t,
		std::shared_ptr<completion_notifier> notifier
	) override
	{
		m_tasks->submit(std::move(t), std::move(notifier));
	}

private:
	std::unique_ptr<thread_pool_executor> m_tasks;
	gate &m_destroyed;
};

} // anonymous namespace

TEST_CASE(
	"a file_transfer_pool::implementation whose executor is let go last on "
	"one of its own threads destroys it on another",
	"[file_transfer_pool]"
)
{
	// The executor holds the executor of computations, which is let go once
	// it has been destroyed.
	gate destroyed;
	gate released;
	{
		const file_transfer_pool::implementation pool(
			std::make_shared<watched_executor>(destroyed)
		);
		const auto executor = pool.get_executor();

		// An operation that waits, on the thread of the executor, until
		// nothing else holds the executor, and reports to a notifier that
		// holds it.
		auto operation = std::make_unique<mock_file_transfer_operation>();
		operation->expectations.push_back(
			NAMED_REQUIRE_CALL(*operation, start(trompeloeil::_))
				.LR_SIDE_EFFECT( released.wait() )
		);
		executor->submit(
			std::move(operation),
			std::make_shared<holding_notifier>(executor)
		);
	}
	released.open();

	CHECK( destroyed.wait_for(std::chrono::seconds(10)) );
}
