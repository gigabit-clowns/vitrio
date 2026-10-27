// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/raw/queued/writable_image_file.hpp>

#include <formats/raw/create_image_file.hpp>
#include <system/scattered_file_write.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/const_memory_range.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

using namespace vitrio;

TEST_CASE(
	"a writable_image_file opens a file for writing, and what is written "
	"to it is there once it is closed",
	"[writable_image_file]"
)
{
	const scoped_path path("writable_image_file.raw");
	create_image_file(path.get(), 8);

	{
		const writable_image_file file(path.get());
		CHECK( file.get_path() == path.get() );

		const std::vector<byte> values(4, static_cast<byte>('w'));
		const const_memory_range buffer(values.data(), values.size());
		std::error_code error;
		write_scattered(file.get_file(), 2, make_span(&buffer, 1), error);
		REQUIRE_FALSE( error );
	}

	std::ifstream input(path.get(), std::ios::binary);
	const std::string bytes(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>()
	);
	CHECK( bytes.substr(2, 4) == "wwww" );
}

TEST_CASE(
	"a writable_image_file refuses a file it can not open, naming it",
	"[writable_image_file]"
)
{
	const scoped_path path("writable_image_file_missing.raw");

	REQUIRE_THROWS_MATCHES(
		writable_image_file(path.get()),
		image_file_error,
		Catch::Matchers::MessageMatches(
			Catch::Matchers::StartsWith(path.get() + ": ")
		)
	);
}
