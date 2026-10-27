// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <file_transfer/file_transfer_worker.hpp>

#include "../fixtures/mapped_test_file.hpp"
#include "../mock/mock_executor.hpp"
#include "../mock/mock_file_transfer_port.hpp"
#include "../mock/mock_file_transfer_operation.hpp"

#include <system/blocking_file_transfer_port.hpp>
#include <system/file_transfer_completion.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/memory_range.hpp>

#include <boost/optional.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <system_error>
#include <thread>
#include <vector>

#include <trompeloeil.hpp>

using namespace vitrio;

TEST_CASE(
	"a file_transfer_worker carries out operations on a thread of its own",
	"[file_transfer_worker]"
)
{
	file_transfer_worker worker(
		std::make_unique<blocking_file_transfer_port>(),
		std::make_shared<mock_executor>()
	);

	std::thread::id thread;
	auto operation = std::make_unique<mock_file_transfer_operation>();
	operation->expectations.push_back(
		NAMED_REQUIRE_CALL(*operation, start(trompeloeil::_))
			.LR_SIDE_EFFECT( thread = std::this_thread::get_id() )
	);

	const auto done = std::make_shared<counting_completion>(1);
	worker.submit(std::move(operation), done);
	CHECK_NOTHROW( done->get() );
	CHECK( thread != std::this_thread::get_id() );
}

TEST_CASE(
	"destroying a file_transfer_worker waits for the operations it was "
	"handed",
	"[file_transfer_worker]"
)
{
	const mapped_test_file file("file_transfer_worker_destroyed.raw", 1000);
	std::vector<byte> buffer(100);
	const auto done = std::make_shared<counting_completion>(1);

	{
		file_transfer_worker worker(
			std::make_unique<blocking_file_transfer_port>(),
			std::make_shared<mock_executor>()
		);

		auto operation = std::make_unique<mock_file_transfer_operation>();
		auto &started = *operation;
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
				.LR_SIDE_EFFECT(
					const memory_range range(buffer.data(), buffer.size());
					_1.queue_read(file.get_file(), 0, make_span(&range, 1), 0)
				)
		);
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(
				started,
				on_transfer(trompeloeil::_, trompeloeil::_)
			)
				.WITH( _1.get_size() == 100 )
		);
		worker.submit(std::move(operation), done);
	}

	REQUIRE( done->is_ready() );
	CHECK_NOTHROW( done->get() );
	const auto bytes = file.get_bytes();
	CHECK( buffer == std::vector<byte>(bytes.begin(), bytes.begin() + 100) );
}

TEST_CASE(
	"a file_transfer_worker waits out a port the kernel is short of memory "
	"for",
	"[file_transfer_worker]"
)
{
	auto port = std::make_unique<mock_file_transfer_port>();
	auto &mocked = *port;

	// The thread waits on the port before the read is queued too, and finds
	// nothing.
	std::atomic<bool> queued(false);
	std::atomic<std::uint64_t> route(0);
	mocked.expectations.push_back(NAMED_ALLOW_CALL(mocked, wake()));
	mocked.expectations.push_back(
		NAMED_ALLOW_CALL(mocked, wait())
			.RETURN( boost::optional<file_transfer_completion>() )
	);
	mocked.expectations.push_back(
		NAMED_REQUIRE_CALL(mocked, wait())
			.LR_WITH( queued.load() )
			.LR_RETURN( file_transfer_completion(route.load(), 5) )
	);
	mocked.expectations.push_back(
		NAMED_REQUIRE_CALL(mocked, wait())
			.LR_WITH( queued.load() )
			.THROW( std::system_error(
				std::make_error_code(std::errc::resource_unavailable_try_again),
				"wait"
			) )
	);
	mocked.expectations.push_back(
		NAMED_REQUIRE_CALL(
			mocked,
			queue_read(
				trompeloeil::_,
				trompeloeil::_,
				trompeloeil::_,
				trompeloeil::_
			)
		)
			.LR_SIDE_EFFECT(
				route = _4;
				queued = true
			)
	);

	auto operation = std::make_unique<mock_file_transfer_operation>();
	auto &started = *operation;
	started.expectations.push_back(
		NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
			.SIDE_EFFECT(
				_1.queue_read(file_handle_t(), 0, span<const memory_range>(), 9)
			)
	);
	started.expectations.push_back(
		NAMED_REQUIRE_CALL(started, on_transfer(trompeloeil::_, trompeloeil::_))
			.WITH( _1.get_tag() == 9 && _1.get_size() == 5 )
	);

	const auto done = std::make_shared<counting_completion>(1);
	{
		file_transfer_worker worker(
			std::move(port),
			std::make_shared<mock_executor>()
		);
		worker.submit(std::move(operation), done);
		CHECK_NOTHROW( done->get() );
	}
}
