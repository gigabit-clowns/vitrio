// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/raw/queued/file_read_plan_execution.hpp>

#include "../../../mock/mock_file_transfer_submitter.hpp"

#include <formats/raw/queued/file_read_plan.hpp>
#include <system/file_transfer_completion.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/memory_range.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <trompeloeil.hpp>

using namespace vitrio;

namespace
{

const std::string path = "file_read_plan_execution.raw";

// What was asked of one read.
struct queued_read
{
	std::uint64_t offset;
	std::uint64_t tag;
	std::size_t buffer_count;
};

// A plan of `reads_per_step` reads a step, each of `size` bytes, read after
// read 1000 bytes apart.
file_read_plan plan_steps(
	std::vector<std::vector<byte>> &buffers,
	std::size_t step_count,
	std::size_t reads_per_step,
	std::size_t size
)
{
	file_read_plan plan;
	buffers.assign(step_count * reads_per_step, std::vector<byte>(size));
	for (std::size_t read = 0; read < buffers.size(); ++read)
	{
		const memory_range buffer(buffers[read].data(), size);
		plan.add_read(
			read / reads_per_step,
			read * 1000,
			make_span(&buffer, 1)
		);
	}

	return plan;
}

// A submitter that takes every read, and keeps what was asked of each.
class recorded_reads
{
public:
	recorded_reads()
		: m_taking(
			NAMED_ALLOW_CALL(
				m_submitter,
				queue_read(
					trompeloeil::_,
					trompeloeil::_,
					trompeloeil::_,
					trompeloeil::_
				)
			).LR_SIDE_EFFECT( m_reads.push_back({_2, _4, _3.size()}) )
		)
	{
	}

	file_transfer_submitter& get_submitter() noexcept
	{
		return m_submitter;
	}

	const std::vector<queued_read>& get_reads() const noexcept
	{
		return m_reads;
	}

private:
	mock_file_transfer_submitter m_submitter;
	std::vector<queued_read> m_reads;
	std::unique_ptr<trompeloeil::expectation> m_taking;
};

} // anonymous namespace

TEST_CASE(
	"a file_read_plan_execution sends the first two steps as it starts, each "
	"read tagged with its index",
	"[file_read_plan_execution]"
)
{
	std::vector<std::vector<byte>> buffers;
	const auto plan = plan_steps(buffers, 3, 2, 100);
	recorded_reads reads;

	file_read_plan_execution execution(path, file_handle_t(), plan);
	CHECK( reads.get_reads().empty() );

	execution.start(reads.get_submitter());
	const auto &sent = reads.get_reads();
	REQUIRE( sent.size() == 4 );
	for (std::uint64_t read = 0; read < sent.size(); ++read)
	{
		CHECK( sent[read].offset == read * 1000 );
		CHECK( sent[read].tag == read );
		CHECK( sent[read].buffer_count == 1 );
	}
}

TEST_CASE(
	"a file_read_plan_execution sends a step once every read of the one two "
	"before it is done with",
	"[file_read_plan_execution]"
)
{
	std::vector<std::vector<byte>> buffers;
	const auto plan = plan_steps(buffers, 4, 2, 100);
	recorded_reads reads;
	auto &submitter = reads.get_submitter();

	file_read_plan_execution execution(path, file_handle_t(), plan);
	execution.start(submitter);
	REQUIRE( reads.get_reads().size() == 4 );

	// One read of the first step done with leaves the third step unsent.
	execution.release(1, submitter);
	CHECK( reads.get_reads().size() == 4 );

	// The reads of the second step done with, the first is not.
	execution.release(2, submitter);
	execution.release(3, submitter);
	CHECK( reads.get_reads().size() == 4 );

	// Now both are, so the third and the fourth step go.
	execution.release(0, submitter);
	CHECK( reads.get_reads().size() == 8 );

	for (std::size_t read = 4; read < 8; ++read)
	{
		execution.release(read, submitter);
	}
	CHECK( reads.get_reads().size() == 8 );
}

TEST_CASE(
	"a file_read_plan_execution takes the completion of a read that arrived "
	"whole",
	"[file_read_plan_execution]"
)
{
	std::vector<std::vector<byte>> buffers;
	const auto plan = plan_steps(buffers, 2, 1, 100);
	const file_read_plan_execution execution(path, file_handle_t(), plan);

	CHECK( execution.take(file_transfer_completion(1, 100)) == 1 );
	CHECK( execution.take(file_transfer_completion(0, 100)) == 0 );
}

TEST_CASE(
	"a file_read_plan_execution fails a read that does not arrive whole, "
	"naming the file",
	"[file_read_plan_execution]"
)
{
	std::vector<std::vector<byte>> buffers;
	const auto plan = plan_steps(buffers, 2, 1, 100);
	const file_read_plan_execution execution(path, file_handle_t(), plan);
	const auto names_the_file = Catch::Matchers::MessageMatches(
		Catch::Matchers::StartsWith(path + ": ")
	);

	SECTION( "a stretch the file ends before" )
	{
		REQUIRE_THROWS_MATCHES(
			execution.take(file_transfer_completion(1, 99)),
			image_file_format_error,
			names_the_file
		);
	}

	SECTION( "a read the system fails" )
	{
		REQUIRE_THROWS_MATCHES(
			execution.take(
				file_transfer_completion(
					0,
					std::make_error_code(std::errc::io_error)
				)
			),
			image_file_error,
			names_the_file
		);
	}
}

TEST_CASE(
	"a file_read_plan_execution of a plan of no read sends none",
	"[file_read_plan_execution]"
)
{
	const file_read_plan plan;
	recorded_reads reads;

	file_read_plan_execution execution(path, file_handle_t(), plan);
	execution.start(reads.get_submitter());
	CHECK( reads.get_reads().empty() );
}
