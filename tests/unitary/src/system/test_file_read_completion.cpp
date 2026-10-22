// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/file_read_completion.hpp>

#include <system_error>

using namespace vitrio;

TEST_CASE(
	"a file_read_completion holds how its read ended",
	"[file_read_completion]"
)
{
	SECTION( "a read that succeeded" )
	{
		const file_read_completion completion(3, 4096);

		CHECK( completion.get_tag() == 3 );
		CHECK( completion.get_size() == 4096 );
		CHECK_FALSE( completion.get_error() );
	}

	SECTION( "a read the system failed" )
	{
		const auto error = std::make_error_code(std::errc::io_error);
		const file_read_completion completion(5, error);

		CHECK( completion.get_tag() == 5 );
		CHECK( completion.get_size() == 0 );
		CHECK( completion.get_error() == error );
	}
}
