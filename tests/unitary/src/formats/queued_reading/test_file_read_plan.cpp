// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/queued_reading/file_read_plan.hpp>

#include <vitrio/memory_range.hpp>

#include <array>
#include <vector>

using namespace vitrio;

TEST_CASE(
	"a file_read_plan holds the reads added to it, in their steps",
	"[file_read_plan]"
)
{
	std::array<char, 64> memory{};
	file_read_plan plan;

	SECTION( "none at first" )
	{
		CHECK( plan.get_step_count() == 0 );
		CHECK( plan.get_read_count() == 0 );
	}

	SECTION( "each with its step, its stretch and its buffers" )
	{
		const std::vector<memory_range> scattered = {
			{memory.data(), 10},
			{memory.data() + 20, 5},
		};
		const memory_range single(memory.data() + 40, 7);

		plan.add_read(0, 100, make_span(scattered));
		plan.add_read(0, 200, make_span(&single, 1));
		plan.add_read(2, 300, make_span(&single, 1));

		CHECK( plan.get_step_count() == 3 );
		REQUIRE( plan.get_read_count() == 3 );

		CHECK( plan.get_step(0) == 0 );
		CHECK( plan.get_offset(0) == 100 );
		CHECK( plan.get_size(0) == 15 );
		const auto buffers = plan.get_buffers(0);
		REQUIRE( buffers.size() == 2 );
		CHECK( buffers[0].get_address() == memory.data() );
		CHECK( buffers[1].get_address() == memory.data() + 20 );
		CHECK( buffers[1].get_size() == 5 );

		CHECK( plan.get_step(1) == 0 );
		CHECK( plan.get_offset(1) == 200 );
		CHECK( plan.get_size(1) == 7 );

		CHECK( plan.get_step(2) == 2 );
		CHECK( plan.get_offset(2) == 300 );
		CHECK( plan.get_buffers(2)[0].get_address() == memory.data() + 40 );
	}
}
