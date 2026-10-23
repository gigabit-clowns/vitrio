// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <file_transfer/thread_pool_file_transfer_executor.hpp>

#include "../fixtures/file_transfer_executor_contract.hpp"
#include "../mock/mock_completion_notifier.hpp"
#include "../mock/mock_executor.hpp"
#include "../mock/mock_file_transfer_operation.hpp"

#include <system/blocking_file_read_port.hpp>
#if defined(VITRIO_ENABLE_IO_URING)
	#include <system/io_uring_file_read_port.hpp>
#endif

#include <vitrio/concurrency/counting_completion.hpp>

#include <cstddef>
#include <memory>
#include <set>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#include <trompeloeil.hpp>

using namespace vitrio;

namespace
{

std::vector<std::unique_ptr<file_read_port>> make_blocking_ports(
	std::size_t count
)
{
	std::vector<std::unique_ptr<file_read_port>> ports;
	for (std::size_t index = 0; index < count; ++index)
	{
		ports.push_back(std::make_unique<blocking_file_read_port>());
	}
	return ports;
}

#if defined(VITRIO_ENABLE_IO_URING)

// Ports with room for four reads each, so that the cases queue more than
// they hold.
std::vector<std::unique_ptr<file_read_port>> make_io_uring_ports(
	std::size_t count
)
{
	std::vector<std::unique_ptr<file_read_port>> ports;
	for (std::size_t index = 0; index < count; ++index)
	{
		ports.push_back(std::make_unique<io_uring_file_read_port>(4));
	}
	return ports;
}

// Whether the kernel opens a ring for this process, which one without
// io_uring, one that has it turned off, and a sandbox that filters its calls
// each refuse.
bool opens_a_ring()
{
	try
	{
		make_io_uring_ports(1);
		return true;
	}
	catch (const std::system_error &)
	{
		return false;
	}
}

#endif

} // anonymous namespace

TEST_CASE(
	"thread_pool_file_transfer_executor needs an executor of tasks and "
	"ports",
	"[thread_pool_file_transfer_executor]"
)
{
	const auto tasks = std::make_shared<mock_executor>();

	SECTION( "a null executor of tasks" )
	{
		CHECK_THROWS_AS(
			thread_pool_file_transfer_executor(nullptr, make_blocking_ports(1)),
			std::invalid_argument
		);
	}

	SECTION( "no port" )
	{
		CHECK_THROWS_AS(
			thread_pool_file_transfer_executor(tasks, make_blocking_ports(0)),
			std::invalid_argument
		);
	}

	SECTION( "a null port" )
	{
		auto ports = make_blocking_ports(2);
		ports[1].reset();
		CHECK_THROWS_AS(
			thread_pool_file_transfer_executor(tasks, std::move(ports)),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"thread_pool_file_transfer_executor refuses a null operation or notifier",
	"[thread_pool_file_transfer_executor]"
)
{
	thread_pool_file_transfer_executor executor(
		std::make_shared<mock_executor>(),
		make_blocking_ports(1)
	);

	CHECK_THROWS_AS(
		executor.submit(nullptr, std::make_shared<mock_completion_notifier>()),
		std::invalid_argument
	);
	CHECK_THROWS_AS(
		executor.submit(
			std::make_unique<mock_file_transfer_operation>(),
			nullptr
		),
		std::invalid_argument
	);
}

TEST_CASE(
	"thread_pool_file_transfer_executor hands the operations to its threads "
	"in turn",
	"[thread_pool_file_transfer_executor]"
)
{
	thread_pool_file_transfer_executor executor(
		std::make_shared<mock_executor>(),
		make_blocking_ports(3)
	);

	// Each operation records the thread it starts on.
	std::vector<std::thread::id> threads(6);
	const auto done = std::make_shared<counting_completion>(threads.size());
	for (std::size_t index = 0; index < threads.size(); ++index)
	{
		auto *const thread = &threads[index];
		auto operation = std::make_unique<mock_file_transfer_operation>();
		operation->expectations.push_back(
			NAMED_REQUIRE_CALL(*operation, start(trompeloeil::_))
				.SIDE_EFFECT( *thread = std::this_thread::get_id() )
		);
		executor.submit(std::move(operation), done);
	}
	done->get();

	const std::set<std::thread::id> distinct(threads.begin(), threads.end());
	CHECK( distinct.size() == 3 );
	for (std::size_t index = 0; index < 3; ++index)
	{
		CHECK( threads[index] == threads[index + 3] );
		CHECK( threads[index] != std::this_thread::get_id() );
	}
}

TEST_CASE(
	"thread_pool_file_transfer_executor with blocking ports keeps the "
	"contract of a file_transfer_executor",
	"[thread_pool_file_transfer_executor]"
)
{
	SECTION( "one thread" )
	{
		check_file_transfer_executor_contract(
			"thread_pool_file_transfer_executor_blocking_1.raw",
			[](std::shared_ptr<executor> tasks)
			{
				return std::make_unique<thread_pool_file_transfer_executor>(
					std::move(tasks),
					make_blocking_ports(1)
				);
			}
		);
	}

	SECTION( "two threads" )
	{
		check_file_transfer_executor_contract(
			"thread_pool_file_transfer_executor_blocking_2.raw",
			[](std::shared_ptr<executor> tasks)
			{
				return std::make_unique<thread_pool_file_transfer_executor>(
					std::move(tasks),
					make_blocking_ports(2)
				);
			}
		);
	}
}

#if defined(VITRIO_ENABLE_IO_URING)

TEST_CASE(
	"thread_pool_file_transfer_executor with io_uring ports keeps the "
	"contract of a file_transfer_executor",
	"[thread_pool_file_transfer_executor]"
)
{
	if (!opens_a_ring())
	{
		SKIP( "The kernel does not open an io_uring here." );
	}

	SECTION( "one thread" )
	{
		check_file_transfer_executor_contract(
			"thread_pool_file_transfer_executor_io_uring_1.raw",
			[](std::shared_ptr<executor> tasks)
			{
				return std::make_unique<thread_pool_file_transfer_executor>(
					std::move(tasks),
					make_io_uring_ports(1)
				);
			}
		);
	}

	SECTION( "two threads" )
	{
		check_file_transfer_executor_contract(
			"thread_pool_file_transfer_executor_io_uring_2.raw",
			[](std::shared_ptr<executor> tasks)
			{
				return std::make_unique<thread_pool_file_transfer_executor>(
					std::move(tasks),
					make_io_uring_ports(2)
				);
			}
		);
	}
}

#endif
