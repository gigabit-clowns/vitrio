// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/concurrency/thread_pool_executor.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using namespace vitrio;

namespace
{

class lambda_task final : public task
{
public:
	explicit lambda_task(std::function<void()> body)
		: m_body(std::move(body))
	{
	}

	void run() override
	{
		m_body();
	}

private:
	std::function<void()> m_body;
};

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

// A notifier that holds what it is given until it is destroyed.
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

// A task that opens a gate when it is destroyed, having run or not.
class opening_task final : public task
{
public:
	explicit opening_task(std::shared_ptr<gate> opened) noexcept
		: m_opened(std::move(opened))
	{
	}

	~opening_task() override
	{
		m_opened->open();
	}

	void run() override
	{
	}

private:
	std::shared_ptr<gate> m_opened;
};

} // anonymous namespace

TEST_CASE(
	"thread_pool_executor needs at least one worker",
	"[thread_pool_executor]"
)
{
	REQUIRE_THROWS_AS( thread_pool_executor(0), std::invalid_argument );
}

TEST_CASE(
	"thread_pool_executor rejects a null task or notifier",
	"[thread_pool_executor]"
)
{
	thread_pool_executor executor(1);
	auto completion = std::make_shared<counting_completion>(1);

	SECTION( "a null task" )
	{
		REQUIRE_THROWS_AS(
			executor.submit(nullptr, completion),
			std::invalid_argument
		);
	}

	SECTION( "a null notifier" )
	{
		REQUIRE_THROWS_AS(
			executor.submit(
				std::make_unique<lambda_task>([] {}),
				nullptr
			),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"thread_pool_executor runs a submitted task and reports success",
	"[thread_pool_executor]"
)
{
	thread_pool_executor executor(2);
	std::atomic<bool> ran(false);

	auto completion = std::make_shared<counting_completion>(1);
	executor.submit(
		std::make_unique<lambda_task>([&ran] { ran.store(true); }),
		completion
	);

	completion->wait();

	CHECK( ran.load() );
	CHECK_NOTHROW( completion->get() );
}

TEST_CASE(
	"thread_pool_executor catches what a task throws and reports it",
	"[thread_pool_executor]"
)
{
	thread_pool_executor executor(2);

	auto completion = std::make_shared<counting_completion>(1);
	executor.submit(
		std::make_unique<lambda_task>(
			[] { throw std::runtime_error("from a task"); }
		),
		completion
	);

	completion->wait();

	REQUIRE_THROWS_AS( completion->get(), std::runtime_error );
}

TEST_CASE(
	"thread_pool_executor lets submitted tasks run concurrently",
	"[thread_pool_executor]"
)
{
	// Each task spins until every other one has also entered. A pool that
	// actually serialized tasks would deadlock the first one here rather
	// than merely running slowly, which is what makes this observable
	// rather than timing-dependent.
	static constexpr std::size_t worker_count = 4;
	thread_pool_executor executor(worker_count);

	std::atomic<std::size_t> entered(0);

	auto completion = std::make_shared<counting_completion>(worker_count);
	std::vector<std::unique_ptr<task>> tasks;
	for (std::size_t i = 0; i < worker_count; ++i)
	{
		tasks.emplace_back(
			std::make_unique<lambda_task>(
				[&entered]
				{
					entered.fetch_add(1);
					while (entered.load() < worker_count)
					{
						std::this_thread::yield();
					}
				}
			)
		);
	}

	for (auto &t : tasks)
	{
		executor.submit(std::move(t), completion);
	}

	completion->wait();

	CHECK( entered.load() == worker_count );
	CHECK_NOTHROW( completion->get() );
}

TEST_CASE(
	"thread_pool_executor exposes its worker count",
	"[thread_pool_executor]"
)
{
	CHECK( thread_pool_executor(3).get_worker_count() == 3 );
}

TEST_CASE(
	"thread_pool_executor let go last on one of its own threads is destroyed "
	"on another, once its tasks have run",
	"[thread_pool_executor]"
)
{
	// The gates are shared, so that no thread of the executor reaches into
	// the frame of the case after it returns.
	const auto released = std::make_shared<gate>();
	const auto ran_after = std::make_shared<gate>();
	{
		const auto executor = std::make_shared<thread_pool_executor>(1);

		// A task that waits until nothing else holds the executor, reported
		// to a notifier that holds it, and one that runs after it.
		executor->submit(
			std::make_unique<lambda_task>(
				[released]
				{
					released->wait_for(std::chrono::seconds(10));
				}
			),
			std::make_shared<holding_notifier>(executor)
		);
		executor->submit(
			std::make_unique<opening_task>(ran_after),
			std::make_shared<counting_completion>(1)
		);
	}
	released->open();

	CHECK( ran_after->wait_for(std::chrono::seconds(10)) );
}
