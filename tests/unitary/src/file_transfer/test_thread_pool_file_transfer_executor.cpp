// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <file_transfer/thread_pool_file_transfer_executor.hpp>

#include "../fixtures/file_transfer_executor_contract.hpp"
#include "../mock/mock_completion_notifier.hpp"
#include "../mock/mock_executor.hpp"
#include "../mock/mock_file_read_port_opener.hpp"
#include "../mock/mock_file_transfer_operation.hpp"

#include <system/asynchronous_file_read_port_opener.hpp>
#include <system/blocking_file_read_port.hpp>
#include <system/blocking_file_read_port_opener.hpp>

#include <vitrio/concurrency/counting_completion.hpp>

#include <cstddef>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

#include <trompeloeil.hpp>

using namespace vitrio;

TEST_CASE(
	"thread_pool_file_transfer_executor needs an executor of tasks and a "
	"thread",
	"[thread_pool_file_transfer_executor]"
)
{
	const blocking_file_read_port_opener ports;

	CHECK_THROWS_AS(
		thread_pool_file_transfer_executor(nullptr, ports, 1),
		std::invalid_argument
	);
	CHECK_THROWS_AS(
		thread_pool_file_transfer_executor(
			std::make_shared<mock_executor>(),
			ports,
			0
		),
		std::invalid_argument
	);
}

TEST_CASE(
	"thread_pool_file_transfer_executor opens a port for each of its threads",
	"[thread_pool_file_transfer_executor]"
)
{
	const mock_file_read_port_opener ports;
	REQUIRE_CALL(ports, open())
		.TIMES(3)
		.RETURN(
			std::unique_ptr<file_read_port>(
				std::make_unique<blocking_file_read_port>()
			)
		);

	const thread_pool_file_transfer_executor executor(
		std::make_shared<mock_executor>(),
		ports,
		3
	);
}

TEST_CASE(
	"thread_pool_file_transfer_executor refuses a null operation or notifier",
	"[thread_pool_file_transfer_executor]"
)
{
	thread_pool_file_transfer_executor executor(
		std::make_shared<mock_executor>(),
		blocking_file_read_port_opener(),
		1
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
		blocking_file_read_port_opener(),
		3
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
					blocking_file_read_port_opener(),
					1
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
					blocking_file_read_port_opener(),
					2
				);
			}
		);
	}
}

TEST_CASE(
	"thread_pool_file_transfer_executor with asynchronous ports keeps the "
	"contract of a file_transfer_executor",
	"[thread_pool_file_transfer_executor]"
)
{
	if (!asynchronous_file_read_port_opener::is_supported())
	{
		SKIP( "The process can not have asynchronous ports here." );
	}

	// Ports with room for four reads each, so that the cases queue more than
	// they hold.
	SECTION( "one thread" )
	{
		check_file_transfer_executor_contract(
			"thread_pool_file_transfer_executor_asynchronous_1.raw",
			[](std::shared_ptr<executor> tasks)
			{
				return std::make_unique<thread_pool_file_transfer_executor>(
					std::move(tasks),
					asynchronous_file_read_port_opener(4),
					1
				);
			}
		);
	}

	SECTION( "two threads" )
	{
		check_file_transfer_executor_contract(
			"thread_pool_file_transfer_executor_asynchronous_2.raw",
			[](std::shared_ptr<executor> tasks)
			{
				return std::make_unique<thread_pool_file_transfer_executor>(
					std::move(tasks),
					asynchronous_file_read_port_opener(4),
					2
				);
			}
		);
	}
}
