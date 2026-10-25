// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/raw/queued/file_write_plan.hpp>

#include <system/file_transfer_completion.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/const_memory_range.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <system_error>
#include <vector>

using namespace vitrio;

TEST_CASE(
	"a file_write_plan holds its writes in the groups they were added to",
	"[file_write_plan]"
)
{
	const std::vector<byte> values(64);
	file_write_plan plan;
	CHECK( plan.get_group_count() == 0 );
	CHECK( plan.get_write_count() == 0 );

	const std::vector<const_memory_range> gathered = {
		{values.data(), 8},
		{values.data() + 16, 4},
	};
	plan.add_group();
	plan.add_write(100, make_span(gathered));
	plan.add_write(200, make_span(gathered.data(), 1));
	plan.add_group();
	plan.add_group();
	plan.add_write(300, make_span(gathered.data() + 1, 1));

	REQUIRE( plan.get_group_count() == 3 );
	REQUIRE( plan.get_write_count() == 3 );

	CHECK( plan.get_first_write(0) == 0 );
	CHECK( plan.get_write_count(0) == 2 );
	CHECK( plan.get_write_count(1) == 0 );
	CHECK( plan.get_first_write(2) == 2 );
	CHECK( plan.get_write_count(2) == 1 );

	CHECK( plan.get_group(1) == 0 );
	CHECK( plan.get_group(2) == 2 );
	CHECK( plan.get_offset(0) == 100 );
	CHECK( plan.get_size(0) == 12 );
	CHECK( plan.get_offset(2) == 300 );
	CHECK( plan.get_size(2) == 4 );

	const auto buffers = plan.get_buffers(0);
	REQUIRE( buffers.size() == 2 );
	CHECK( buffers[1].get_address() == values.data() + 16 );
	CHECK( buffers[1].get_size() == 4 );
}

TEST_CASE(
	"take_write names the write a completion is of, and refuses one the "
	"system failed",
	"[file_write_plan]"
)
{
	const std::vector<byte> values(8);
	const const_memory_range buffer(values.data(), values.size());
	file_write_plan plan;
	plan.add_group();
	plan.add_write(0, make_span(&buffer, 1));
	plan.add_write(8, make_span(&buffer, 1));

	CHECK( take_write("file.raw", plan, file_transfer_completion(1, 8)) == 1 );
	REQUIRE_THROWS_MATCHES(
		take_write(
			"file.raw",
			plan,
			file_transfer_completion(
				0,
				std::make_error_code(std::errc::no_space_on_device)
			)
		),
		image_file_error,
		Catch::Matchers::MessageMatches(
			Catch::Matchers::StartsWith("file.raw: ")
		)
	);
}
