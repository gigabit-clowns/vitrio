// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/raw/create_image_file.hpp>

#include <vitrio/exceptions/image_file_error.hpp>

#include <vitrio/tests/scoped_path.hpp>

#include <boost/filesystem/operations.hpp>

#include <fstream>
#include <string>
#include <vector>

using namespace vitrio;

namespace
{

void write_file(const std::string &path, const std::vector<char> &contents)
{
	std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
	output.write(contents.data(), static_cast<std::streamsize>(
		contents.size()));
}

std::size_t size_on_disk(const std::string &path)
{
	return static_cast<std::size_t>(boost::filesystem::file_size(path));
}

} // anonymous namespace

TEST_CASE( "a file is laid out in full before it is written",
	"[create_image_file]" )
{
	const scoped_path path("create_image_file.tmp");

	SECTION( "it is created at exactly the size asked for" )
	{
		create_image_file(path.get(), 2048);

		REQUIRE( size_on_disk(path.get()) == 2048 );
	}

	SECTION( "a file already there is replaced" )
	{
		write_file(path.get(), std::vector<char>(8192, 'x'));
		create_image_file(path.get(), 1024);

		REQUIRE( size_on_disk(path.get()) == 1024 );
	}

	SECTION( "a file of no bytes is refused" )
	{
		REQUIRE_THROWS_MATCHES(
			create_image_file(path.get(), 0),
			image_file_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(path.get() + ": ")
			)
		);
	}
}
