// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/memory_mapping/image_file_mapping.hpp>

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

std::vector<char> read_file(const std::string &path)
{
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	return std::vector<char>(
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()
	);
}

std::size_t size_on_disk(const std::string &path)
{
	return static_cast<std::size_t>(boost::filesystem::file_size(path));
}

} // anonymous namespace

TEST_CASE( "a file is laid out in full before it is mapped",
	"[image_file_mapping]" )
{
	const scoped_path path("mapping_layout.tmp");

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

TEST_CASE( "a file is read through its mapping",
	"[image_file_mapping]" )
{
	const scoped_path path("mapping_read.tmp");

	SECTION( "the whole file is mapped from its first byte" )
	{
		write_file(path.get(), {'M', 'A', 'P', ' ', '\1', '\2'});

		const image_file_mapping mapping(
			path.get(),
			image_file_access::read_only
		);

		REQUIRE( mapping.get_size() == 6 );
		REQUIRE( mapping.get_data()[0] ==
			static_cast<std::uint8_t>('M') );
		REQUIRE( mapping.get_data()[5] == 2 );
	}

	SECTION( "a file that is not there is refused, naming it" )
	{
		REQUIRE_THROWS_MATCHES(
			image_file_mapping(path.get(), image_file_access::read_only),
			image_file_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(path.get() + ": ")
			)
		);
	}

	SECTION( "an empty file is refused" )
	{
		write_file(path.get(), {});

		REQUIRE_THROWS_MATCHES(
			image_file_mapping(path.get(), image_file_access::read_only),
			image_file_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(path.get() + ": ")
			)
		);
	}
}

TEST_CASE( "a file is written through its mapping",
	"[image_file_mapping]" )
{
	const scoped_path path("mapping_write.tmp");

	SECTION( "what is written reaches the storage" )
	{
		create_image_file(path.get(), 4);

		{
			image_file_mapping mapping(
				path.get(),
				image_file_access::read_write
			);
			auto *data = mapping.get_data();
			data[0] = byte('M');
			data[1] = byte('A');
			data[2] = byte('P');
			data[3] = byte(' ');
			mapping.flush();
		}

		const auto contents = read_file(path.get());

		REQUIRE( contents == std::vector<char>{'M', 'A', 'P', ' '} );
	}

	SECTION( "a second mapping sees what the first one wrote" )
	{
		create_image_file(path.get(), 2);

		image_file_mapping writer(path.get(), image_file_access::read_write);
		writer.get_data()[0] = byte(0x42);
		writer.flush();

		const image_file_mapping reader(
			path.get(),
			image_file_access::read_only
		);

		REQUIRE( reader.get_data()[0] == 0x42 );
	}
}

TEST_CASE( "a mapping carries its file when it is moved",
	"[image_file_mapping]" )
{
	const scoped_path path("mapping_moved.tmp");
	write_file(path.get(), {'a', 'b', 'c', 'd'});

	SECTION( "a moved mapping keeps reading the file" )
	{
		image_file_mapping original(path.get(), image_file_access::read_only);
		const image_file_mapping moved(std::move(original));

		REQUIRE( moved.get_size() == 4 );
		REQUIRE( moved.get_data()[0] ==
			static_cast<std::uint8_t>('a') );
	}

	SECTION( "a mapping assigned over keeps reading the file" )
	{
		const scoped_path other("mapping-other.tmp");
		write_file(other.get(), {'z'});

		image_file_mapping original(path.get(), image_file_access::read_only);
		image_file_mapping target(other.get(), image_file_access::read_only);
		target = std::move(original);

		REQUIRE( target.get_size() == 4 );
		REQUIRE( target.get_data()[0] ==
			static_cast<std::uint8_t>('a') );
	}
}
