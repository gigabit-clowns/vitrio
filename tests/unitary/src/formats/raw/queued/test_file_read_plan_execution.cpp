// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/raw/queued/file_read_plan_execution.hpp>

#include "../../../fixtures/file_read_queue_pools.hpp"
#include "../../../fixtures/mapped_test_file.hpp"
#include "../../../fixtures/no_file.hpp"
#include "../../../fixtures/recording_file_read_queue.hpp"

#include <formats/raw/queued/file_read_plan.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/memory_range.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <vector>

using namespace vitrio;

namespace
{

constexpr std::size_t file_size = 4096;

// A plan of one read a step, each of `size` bytes, `stride` bytes apart.
file_read_plan plan_steps(
	std::vector<std::vector<byte>> &buffers,
	std::size_t step_count,
	std::size_t size,
	std::size_t stride
)
{
	file_read_plan plan;
	buffers.assign(step_count, std::vector<byte>(size));
	for (std::size_t step = 0; step < step_count; ++step)
	{
		const memory_range buffer(buffers[step].data(), size);
		plan.add_read(step, step * stride, make_span(&buffer, 1));
	}

	return plan;
}

bool holds_bytes_at(
	const std::vector<byte> &buffer,
	const mapped_test_file &file,
	std::size_t offset
)
{
	const auto bytes = file.get_bytes();
	return std::equal(buffer.begin(), buffer.end(), bytes.begin() + offset);
}

// Takes every read of an execution.
void take_every_read(file_read_plan_execution &execution)
{
	std::size_t read = 0;
	while (execution.take_next(read))
	{
	}
}

} // anonymous namespace

TEST_CASE(
	"a file_read_plan_execution takes every read of its plan whole",
	"[file_read_plan_execution]"
)
{
	const auto pool = GENERATE(from_range(make_every_file_read_queue_pool()));
	recording_file_read_queue queue(pool->get_queue());
	const mapped_test_file file(
		"file_read_plan_execution_reads.raw",
		file_size
	);

	// Two reads in the first step, the first scattered over two buffers, and
	// one in each of the two steps after it.
	std::vector<byte> first(40);
	std::vector<byte> second(60);
	std::vector<byte> third(50);
	std::vector<byte> fourth(30);
	std::vector<byte> fifth(20);
	const std::vector<memory_range> scattered = {
		{first.data(), first.size()},
		{second.data(), second.size()},
	};
	const memory_range third_buffer(third.data(), third.size());
	const memory_range fourth_buffer(fourth.data(), fourth.size());
	const memory_range fifth_buffer(fifth.data(), fifth.size());

	file_read_plan plan;
	plan.add_read(0, 0, make_span(scattered));
	plan.add_read(0, 500, make_span(&third_buffer, 1));
	plan.add_read(1, 1000, make_span(&fourth_buffer, 1));
	plan.add_read(2, 2000, make_span(&fifth_buffer, 1));

	std::vector<std::size_t> taken;
	{
		file_read_plan_execution execution(
		file.get_path(),
		file.get_file(),
		plan,
		queue
	);
		std::size_t read = 0;
		while (execution.take_next(read))
		{
			taken.push_back(read);
		}
		CHECK_FALSE( execution.take_next(read) );
	}

	std::sort(taken.begin(), taken.end());
	CHECK( taken == std::vector<std::size_t>{0, 1, 2, 3} );
	CHECK( queue.get_outstanding() == 0 );
	CHECK( holds_bytes_at(first, file, 0) );
	CHECK( holds_bytes_at(second, file, 40) );
	CHECK( holds_bytes_at(third, file, 500) );
	CHECK( holds_bytes_at(fourth, file, 1000) );
	CHECK( holds_bytes_at(fifth, file, 2000) );
}

TEST_CASE(
	"a file_read_plan_execution sends a step once the one two before it is "
	"done with",
	"[file_read_plan_execution]"
)
{
	// Reads that read as they are asked complete in the order they were
	// sent, which this relies on.
	blocking_file_read_queue_pool pool;
	recording_file_read_queue queue(pool.get_queue());
	const mapped_test_file file(
		"file_read_plan_execution_steps.raw",
		file_size
	);

	std::vector<std::vector<byte>> buffers;
	const auto plan = plan_steps(buffers, 4, 100, 1000);

	file_read_plan_execution execution(
		file.get_path(),
		file.get_file(),
		plan,
		queue
	);
	CHECK( queue.get_reads().empty() );

	std::size_t read = 0;
	const std::vector<std::size_t> sent_by_then = {2, 3, 4, 4};
	for (std::size_t step = 0; step < sent_by_then.size(); ++step)
	{
		REQUIRE( execution.take_next(read) );
		CHECK( read == step );
		CHECK( queue.get_reads().size() == sent_by_then[step] );
	}
	CHECK_FALSE( execution.take_next(read) );
}

TEST_CASE(
	"a file_read_plan_execution fails reads that do not arrive whole, and "
	"leaves none outstanding",
	"[file_read_plan_execution]"
)
{
	const auto pool = GENERATE(from_range(make_every_file_read_queue_pool()));
	recording_file_read_queue queue(pool->get_queue());
	const mapped_test_file file(
		"file_read_plan_execution_fails.raw",
		file_size
	);
	std::vector<std::vector<byte>> buffers;

	SECTION( "a stretch the file ends before, naming the file" )
	{
		const auto plan = plan_steps(buffers, 3, 100, file_size - 150);

		file_read_plan_execution execution(
			file.get_path(),
			file.get_file(),
			plan,
			queue
		);
		REQUIRE_THROWS_MATCHES(
			take_every_read(execution),
			image_file_format_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(file.get_path() + ": ")
			)
		);
	}

	SECTION( "a read the system fails" )
	{
		const auto plan = plan_steps(buffers, 3, 100, 1000);

		file_read_plan_execution execution(
			file.get_path(),
			no_file,
			plan,
			queue
		);
		std::size_t read = 0;
		REQUIRE_THROWS_AS( execution.take_next(read), std::system_error );
	}

	CHECK( queue.get_outstanding() == 0 );
}

TEST_CASE(
	"destroying a file_read_plan_execution waits for the reads that are "
	"outstanding",
	"[file_read_plan_execution]"
)
{
	const auto pool = GENERATE(from_range(make_every_file_read_queue_pool()));
	recording_file_read_queue queue(pool->get_queue());
	const mapped_test_file file(
		"file_read_plan_execution_waits.raw",
		file_size
	);

	std::vector<std::vector<byte>> buffers;
	const auto plan = plan_steps(buffers, 3, 100, 1000);
	{
		file_read_plan_execution execution(
		file.get_path(),
		file.get_file(),
		plan,
		queue
	);
		std::size_t read = 0;
		REQUIRE( execution.take_next(read) );
	}

	CHECK( queue.get_outstanding() == 0 );
	for (std::size_t step = 0; step < 2; ++step)
	{
		CHECK( holds_bytes_at(buffers[step], file, step * 1000) );
	}
}

TEST_CASE(
	"a file_read_plan_execution of a plan of no read takes none",
	"[file_read_plan_execution]"
)
{
	const auto pool = GENERATE(from_range(make_every_file_read_queue_pool()));
	recording_file_read_queue queue(pool->get_queue());
	const mapped_test_file file(
		"file_read_plan_execution_none.raw",
		file_size
	);
	const file_read_plan plan;

	file_read_plan_execution execution(
		file.get_path(),
		file.get_file(),
		plan,
		queue
	);
	std::size_t read = 0;

	CHECK_FALSE( execution.take_next(read) );
	CHECK( queue.get_reads().empty() );
}
