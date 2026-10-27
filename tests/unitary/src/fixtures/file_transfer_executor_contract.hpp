// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <catch2/catch_test_macros.hpp>

#include "mapped_test_file.hpp"
#include "../mock/mock_completion_notifier.hpp"
#include "../mock/mock_file_transfer_operation.hpp"
#include "../mock/mock_task.hpp"

#include <file_transfer/file_transfer_executor.hpp>

#include <formats/raw/mapped/image_file_access.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/concurrency/thread_pool_executor.hpp>
#include <vitrio/const_memory_range.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <trompeloeil.hpp>

namespace vitrio
{

/**
 * @brief Queue a read of a stretch of a file into a buffer, through the scope
 * of an operation.
 */
inline void queue_into(
	file_transfer_scope &scope,
	file_handle_t file,
	std::uint64_t offset,
	std::vector<byte> &buffer,
	std::uint64_t tag
)
{
	const memory_range range(buffer.data(), buffer.size());
	scope.queue_read(file, offset, make_span(&range, 1), tag);
}

/**
 * @brief Check that the executors a function makes keep the contract of a
 * @ref file_transfer_executor, in a section each.
 *
 * @param name The name of the file the operations read, which no other case
 * of the suite may use, nor the same name followed by ".written", which the
 * operations write.
 * @param make_executor Makes an executor whose tasks run on a given
 * executor, as a std::unique_ptr.
 */
template <typename MakeExecutor>
void check_file_transfer_executor_contract(
	const std::string &name,
	MakeExecutor make_executor
)
{
	constexpr std::size_t file_size = 100000;
	const mapped_test_file file(name, file_size);
	const auto file_bytes = file.get_bytes();
	const auto bytes_at = [&](std::size_t offset, std::size_t size)
	{
		const auto first = file_bytes.begin() + offset;
		return std::vector<byte>(first, first + size);
	};

	// What the operations read into outlives the executor, which may still
	// be reading into it until it is destroyed.
	std::vector<std::vector<byte>> buffers;
	const auto tasks = std::make_shared<thread_pool_executor>(2);
	auto executor = make_executor(tasks);

	SECTION( "an operation is reported once all it reads has landed" )
	{
		buffers.assign(3, std::vector<byte>(100));
		auto operation = std::make_unique<mock_file_transfer_operation>();
		auto &started = *operation;
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
				.LR_SIDE_EFFECT(
					for (std::uint64_t tag = 0; tag < 3; ++tag)
					{
						queue_into(
							_1,
							file.get_file(),
							1000 * tag,
							buffers[tag],
							tag
						);
					}
				)
		);
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(
				started,
				on_transfer(trompeloeil::_, trompeloeil::_)
			)
				.WITH(
					_1.get_tag() < 3 && !_1.get_error() &&
					_1.get_size() == 100
				)
				.TIMES(3)
		);

		const auto done = std::make_shared<counting_completion>(1);
		executor->submit(std::move(operation), done);
		CHECK_NOTHROW( done->get() );
		for (std::uint64_t tag = 0; tag < 3; ++tag)
		{
			CHECK( buffers[tag] == bytes_at(1000 * tag, 100) );
		}
	}

	SECTION( "an operation writes, then syncs what it wrote" )
	{
		const mapped_test_file written(
			name + ".written",
			file_size,
			image_file_access::read_write
		);
		const std::vector<byte> values(300, 7);
		const const_memory_range range(values.data(), values.size());

		auto operation = std::make_unique<mock_file_transfer_operation>();
		auto &started = *operation;
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
				.LR_SIDE_EFFECT(
					_1.queue_write(
						written.get_file(),
						5000,
						make_span(&range, 1),
						0
					)
				)
		);
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(
				started,
				on_transfer(trompeloeil::_, trompeloeil::_)
			)
				.WITH(
					_1.get_tag() == 0 && !_1.get_error() &&
					_1.get_size() == values.size()
				)
				.LR_SIDE_EFFECT( _2.queue_sync(written.get_file(), 1) )
		);
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(
				started,
				on_transfer(trompeloeil::_, trompeloeil::_)
			)
				.WITH( _1.get_tag() == 1 && !_1.get_error() )
		);

		const auto done = std::make_shared<counting_completion>(1);
		executor->submit(std::move(operation), done);
		CHECK_NOTHROW( done->get() );

		const auto bytes = written.get_bytes();
		CHECK(
			std::vector<byte>(
				bytes.begin() + 5000,
				bytes.begin() + 5000 + values.size()
			) == values
		);
	}

	SECTION( "an operation that starts nothing is reported" )
	{
		auto operation = std::make_unique<mock_file_transfer_operation>();
		operation->expectations.push_back(
			NAMED_REQUIRE_CALL(*operation, start(trompeloeil::_))
		);

		const auto done = std::make_shared<counting_completion>(1);
		executor->submit(std::move(operation), done);
		CHECK_NOTHROW( done->get() );
	}

	SECTION( "operations at once are each handed their own completions" )
	{
		constexpr std::size_t operation_count = 32;
		constexpr std::uint64_t reads_per_operation = 8;
		constexpr std::size_t size = 64;
		buffers.assign(
			operation_count * reads_per_operation,
			std::vector<byte>(size)
		);

		// The tags each operation was handed back, a bit each.
		std::vector<unsigned> handed(operation_count, 0);
		const auto done = std::make_shared<counting_completion>(
			operation_count
		);
		for (std::size_t index = 0; index < operation_count; ++index)
		{
			// The calls come on another thread, after the loop has moved on,
			// so they take what they need by value.
			const auto handle = file.get_file();
			auto *const reads = &buffers[index * 8];
			auto *const operation_handed = &handed[index];

			auto operation = std::make_unique<mock_file_transfer_operation>();
			auto &started = *operation;
			started.expectations.push_back(
				NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
					.SIDE_EFFECT(
						for (std::uint64_t tag = 0; tag < 8; ++tag)
						{
							const auto offset = (index * 8 + tag) * 100;
							queue_into(_1, handle, offset, reads[tag], tag);
						}
					)
			);
			started.expectations.push_back(
				NAMED_REQUIRE_CALL(
					started,
					on_transfer(trompeloeil::_, trompeloeil::_)
				)
					.WITH( _1.get_tag() < 8 && _1.get_size() == 64 )
					.SIDE_EFFECT( *operation_handed |= 1u << _1.get_tag() )
					.TIMES(8)
			);
			executor->submit(std::move(operation), done);
		}

		CHECK_NOTHROW( done->get() );
		CHECK( handed == std::vector<unsigned>(operation_count, 0xff) );
		for (std::size_t read = 0; read < buffers.size(); ++read)
		{
			CHECK( buffers[read] == bytes_at(read * 100, size) );
		}
	}

	SECTION( "an operation carries on from its reads" )
	{
		constexpr std::uint64_t step_count = 20;
		buffers.assign(step_count, std::vector<byte>(10));

		auto operation = std::make_unique<mock_file_transfer_operation>();
		auto &started = *operation;
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
				.LR_SIDE_EFFECT(
					queue_into(_1, file.get_file(), 0, buffers[0], 0)
				)
		);
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(
				started,
				on_transfer(trompeloeil::_, trompeloeil::_)
			)
				.LR_SIDE_EFFECT(
					const auto next = _1.get_tag() + 1;
					if (next < step_count)
					{
						queue_into(
							_2,
							file.get_file(),
							next * 10,
							buffers[next],
							next
						);
					}
				)
				.TIMES(step_count)
		);

		const auto done = std::make_shared<counting_completion>(1);
		executor->submit(std::move(operation), done);
		CHECK_NOTHROW( done->get() );
		for (std::uint64_t step = 0; step < step_count; ++step)
		{
			CHECK( buffers[step] == bytes_at(step * 10, 10) );
		}
	}

	SECTION( "a task runs on the executor of computations, and its outcome "
		"comes back to the operation" )
	{
		std::atomic<bool> ran(false);
		std::atomic<bool> ran_before_outcome(false);

		auto work = std::make_unique<mock_task>();
		work->expectations.push_back(
			NAMED_REQUIRE_CALL(*work, run())
				.LR_SIDE_EFFECT( ran = true )
		);

		auto operation = std::make_unique<mock_file_transfer_operation>();
		auto &started = *operation;
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
				.LR_SIDE_EFFECT( _1.run(std::move(work), 3) )
		);
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(
				started,
				on_task(trompeloeil::_, trompeloeil::_, trompeloeil::_)
			)
				.WITH( _1 == 3 && _2 == nullptr )
				.LR_SIDE_EFFECT( ran_before_outcome = ran.load() )
		);

		const auto done = std::make_shared<counting_completion>(1);
		executor->submit(std::move(operation), done);
		CHECK_NOTHROW( done->get() );
		CHECK( ran_before_outcome );
	}

	SECTION( "a task that fails reaches the operation, which may fail with "
		"what it threw" )
	{
		auto work = std::make_unique<mock_task>();
		work->expectations.push_back(
			NAMED_REQUIRE_CALL(*work, run())
				.THROW( std::runtime_error("task") )
		);

		auto operation = std::make_unique<mock_file_transfer_operation>();
		auto &started = *operation;
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
				.LR_SIDE_EFFECT( _1.run(std::move(work), 0) )
		);
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(
				started,
				on_task(trompeloeil::_, trompeloeil::_, trompeloeil::_)
			)
				.WITH( _2 != nullptr )
				.SIDE_EFFECT( std::rethrow_exception(_2) )
		);

		const auto done = std::make_shared<counting_completion>(1);
		executor->submit(std::move(operation), done);
		CHECK_THROWS_AS( done->get(), std::runtime_error );
	}

	SECTION( "a failure is reported once what the operation started has "
		"completed" )
	{
		buffers.assign(4, std::vector<byte>(1000));
		auto operation = std::make_unique<mock_file_transfer_operation>();
		auto &started = *operation;
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
				.LR_SIDE_EFFECT(
					for (std::uint64_t tag = 0; tag < 4; ++tag)
					{
						queue_into(
							_1,
							file.get_file(),
							10000 * tag,
							buffers[tag],
							tag
						);
					}
				)
				.THROW( std::runtime_error("start") )
		);
		started.expectations.push_back(
			NAMED_FORBID_CALL(
				started,
				on_transfer(trompeloeil::_, trompeloeil::_)
			)
		);

		const auto done = std::make_shared<counting_completion>(1);
		executor->submit(std::move(operation), done);
		CHECK_THROWS_AS( done->get(), std::runtime_error );
		for (std::uint64_t tag = 0; tag < 4; ++tag)
		{
			CHECK( buffers[tag] == bytes_at(10000 * tag, 1000) );
		}
	}

	SECTION( "an operation has more reads at once than a port holds" )
	{
		constexpr std::uint64_t read_count = 200;
		buffers.assign(read_count, std::vector<byte>(10));

		auto operation = std::make_unique<mock_file_transfer_operation>();
		auto &started = *operation;
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
				.LR_SIDE_EFFECT(
					for (std::uint64_t tag = 0; tag < read_count; ++tag)
					{
						queue_into(
							_1,
							file.get_file(),
							tag * 10,
							buffers[tag],
							tag
						);
					}
				)
		);
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(
				started,
				on_transfer(trompeloeil::_, trompeloeil::_)
			)
				.WITH( _1.get_size() == 10 )
				.TIMES(read_count)
		);

		const auto done = std::make_shared<counting_completion>(1);
		executor->submit(std::move(operation), done);
		CHECK_NOTHROW( done->get() );
		for (std::uint64_t tag = 0; tag < read_count; ++tag)
		{
			CHECK( buffers[tag] == bytes_at(tag * 10, 10) );
		}
	}

	SECTION( "an executor with nothing to do is woken by a submission" )
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		buffers.assign(1, std::vector<byte>(10));
		auto operation = std::make_unique<mock_file_transfer_operation>();
		auto &started = *operation;
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
				.LR_SIDE_EFFECT(
					queue_into(_1, file.get_file(), 0, buffers[0], 0)
				)
		);
		started.expectations.push_back(
			NAMED_REQUIRE_CALL(
				started,
				on_transfer(trompeloeil::_, trompeloeil::_)
			)
		);

		const auto done = std::make_shared<counting_completion>(1);
		executor->submit(std::move(operation), done);
		CHECK_NOTHROW( done->get() );
	}

	SECTION( "a notifier may submit another operation" )
	{
		buffers.assign(1, std::vector<byte>(10));

		auto first = std::make_unique<mock_file_transfer_operation>();
		first->expectations.push_back(
			NAMED_REQUIRE_CALL(*first, start(trompeloeil::_))
		);

		auto second = std::make_unique<mock_file_transfer_operation>();
		auto &second_started = *second;
		second_started.expectations.push_back(
			NAMED_REQUIRE_CALL(second_started, start(trompeloeil::_))
				.LR_SIDE_EFFECT(
					queue_into(_1, file.get_file(), 0, buffers[0], 0)
				)
		);
		second_started.expectations.push_back(
			NAMED_REQUIRE_CALL(
				second_started,
				on_transfer(trompeloeil::_, trompeloeil::_)
			)
		);

		const auto second_done = std::make_shared<counting_completion>(1);
		const auto first_done = std::make_shared<mock_completion_notifier>();
		REQUIRE_CALL(*first_done, notify_done(trompeloeil::_))
			.WITH( _1 == nullptr )
			.LR_SIDE_EFFECT(
				executor->submit(std::move(second), second_done)
			);

		executor->submit(std::move(first), first_done);
		CHECK_NOTHROW( second_done->get() );
		CHECK( buffers[0] == bytes_at(0, 10) );
	}

	SECTION( "destroying the executor waits for the operations under way" )
	{
		constexpr std::size_t operation_count = 8;
		constexpr std::uint64_t reads_per_operation = 16;
		buffers.assign(
			operation_count * reads_per_operation,
			std::vector<byte>(100)
		);

		const auto done = std::make_shared<counting_completion>(
			operation_count
		);
		for (std::size_t index = 0; index < operation_count; ++index)
		{
			const auto handle = file.get_file();
			auto *const reads = &buffers[index * 16];

			auto operation = std::make_unique<mock_file_transfer_operation>();
			auto &started = *operation;
			started.expectations.push_back(
				NAMED_REQUIRE_CALL(started, start(trompeloeil::_))
					.SIDE_EFFECT(
						for (std::uint64_t tag = 0; tag < 16; ++tag)
						{
							const auto offset = (index * 16 + tag) * 500;
							queue_into(_1, handle, offset, reads[tag], tag);
						}
					)
			);
			started.expectations.push_back(
				NAMED_REQUIRE_CALL(
					started,
					on_transfer(trompeloeil::_, trompeloeil::_)
				)
					.TIMES(16)
			);
			executor->submit(std::move(operation), done);
		}

		executor.reset();
		REQUIRE( done->is_ready() );
		CHECK_NOTHROW( done->get() );
		for (std::size_t read = 0; read < buffers.size(); ++read)
		{
			CHECK( buffers[read] == bytes_at(read * 500, 100) );
		}
	}
}

} // namespace vitrio
