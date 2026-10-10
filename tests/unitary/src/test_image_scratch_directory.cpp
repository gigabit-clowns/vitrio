// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <image_scratch_directory.hpp>

#include "fixtures/scoped_directory.hpp"

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/image_scratch_storage.hpp>

#include <boost/filesystem/operations.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace vitrio;

namespace
{

const std::string extension = ".vitrio-scratch";

// Maps the file of a scratch and lets it go, expecting it to fit.
void make_file(
	image_scratch_directory &directory,
	std::uint64_t fingerprint,
	std::size_t size
)
{
	std::size_t room = 0;
	REQUIRE( directory.acquire(fingerprint, size, room) != nullptr );
}

void write_file(const std::string &path, std::size_t size)
{
	std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
	output << std::string(size, 'x');
}

} // anonymous namespace

TEST_CASE(
	"an image_scratch_directory creates its directory",
	"[image_scratch_directory]"
)
{
	const scoped_directory path("image_scratch_directory_created");

	const image_scratch_directory directory(path.get("nested"), 1024);

	CHECK( boost::filesystem::is_directory(path.get("nested")) );
	CHECK( directory.get_path() == path.get("nested") );
	CHECK( directory.get_budget() == 1024 );
}

TEST_CASE(
	"an image_scratch_directory refuses a path that is a file",
	"[image_scratch_directory]"
)
{
	const scoped_directory path("image_scratch_directory_file");
	boost::filesystem::create_directories(path.get());
	write_file(path.get("file"), 1);

	REQUIRE_THROWS_AS(
		image_scratch_directory(path.get("file"), 1024),
		image_file_error
	);
}

TEST_CASE(
	"an image_scratch_directory names a file after its fingerprint",
	"[image_scratch_directory]"
)
{
	const scoped_directory path("image_scratch_directory_name");
	const image_scratch_directory directory(path.get(), 1024);

	CHECK( directory.get_file_path(0xab) ==
		path.get("00000000000000ab.vitrio-scratch") );
	CHECK( directory.get_file_path(0xfedcba9876543210) ==
		path.get("fedcba9876543210.vitrio-scratch") );
}

TEST_CASE(
	"an image_scratch_directory maps a file of the size it is asked for",
	"[image_scratch_directory]"
)
{
	const scoped_directory path("image_scratch_directory_acquire");
	image_scratch_directory directory(path.get(), 1024);

	std::size_t room = 0;
	const auto storage = directory.acquire(7, 256, room);

	REQUIRE( storage != nullptr );
	CHECK( storage->get_size() == 256 );
	CHECK( boost::filesystem::file_size(directory.get_file_path(7)) == 256 );
	CHECK( room == 1024 );
}

TEST_CASE(
	"an image_scratch_directory gives back what a file held",
	"[image_scratch_directory]"
)
{
	const scoped_directory path("image_scratch_directory_kept");
	image_scratch_directory directory(path.get(), 1024);

	std::size_t room = 0;
	directory.acquire(7, 256, room)->get_data()[100] = 42;

	const auto storage = directory.acquire(7, 256, room);

	REQUIRE( storage != nullptr );
	CHECK( storage->get_data()[100] == 42 );
	CHECK( path.list_files(extension).size() == 1 );
}

TEST_CASE(
	"an image_scratch_directory removes the files used least recently to "
	"make room",
	"[image_scratch_directory]"
)
{
	const scoped_directory path("image_scratch_directory_evict");
	image_scratch_directory directory(path.get(), 300);
	make_file(directory, 1, 100);
	make_file(directory, 2, 100);
	make_file(directory, 3, 100);
	age_file(directory.get_file_path(1), 20);
	age_file(directory.get_file_path(2), 30);
	age_file(directory.get_file_path(3), 10);

	SECTION( "as many as it takes and no more" )
	{
		std::size_t room = 0;
		REQUIRE( directory.acquire(4, 150, room) != nullptr );

		CHECK_FALSE( boost::filesystem::exists(directory.get_file_path(2)) );
		CHECK_FALSE( boost::filesystem::exists(directory.get_file_path(1)) );
		CHECK( boost::filesystem::exists(directory.get_file_path(3)) );
		CHECK( room == 200 );
	}

	SECTION( "none when the file fits beside them" )
	{
		std::size_t room = 0;
		REQUIRE( directory.acquire(1, 100, room) != nullptr );

		CHECK( path.list_files(extension).size() == 3 );
		CHECK( room == 100 );
	}

	SECTION( "and a file it gives back counts as used" )
	{
		make_file(directory, 2, 100);

		std::size_t room = 0;
		REQUIRE( directory.acquire(4, 100, room) != nullptr );

		CHECK( boost::filesystem::exists(directory.get_file_path(2)) );
		CHECK_FALSE( boost::filesystem::exists(directory.get_file_path(1)) );
		CHECK( boost::filesystem::exists(directory.get_file_path(3)) );
	}
}

TEST_CASE(
	"an image_scratch_directory leaves alone what it did not name",
	"[image_scratch_directory]"
)
{
	const scoped_directory path("image_scratch_directory_others");
	image_scratch_directory directory(path.get(), 200);
	write_file(path.get("notes.txt"), 1000);
	age_file(path.get("notes.txt"), 100);

	std::size_t room = 0;
	REQUIRE( directory.acquire(1, 200, room) != nullptr );

	CHECK( boost::filesystem::file_size(path.get("notes.txt")) == 1000 );
	CHECK( room == 200 );
}

TEST_CASE(
	"an image_scratch_directory maps nothing larger than its budget, having "
	"removed every file",
	"[image_scratch_directory]"
)
{
	const scoped_directory path("image_scratch_directory_too_large");
	image_scratch_directory directory(path.get(), 300);
	make_file(directory, 1, 100);

	std::size_t room = 0;
	CHECK( directory.acquire(2, 301, room) == nullptr );

	CHECK( room == 300 );
	CHECK_FALSE( boost::filesystem::exists(directory.get_file_path(1)) );
	CHECK_FALSE( boost::filesystem::exists(directory.get_file_path(2)) );
}

TEST_CASE(
	"an image_scratch_directory stays within its budget when threads make "
	"room at once",
	"[image_scratch_directory]"
)
{
	const scoped_directory path("image_scratch_directory_threads");
	image_scratch_directory directory(path.get(), 400);

	std::vector<std::thread> threads;
	std::atomic<std::size_t> mapped(0);
	for (std::uint64_t fingerprint = 1; fingerprint <= 8; ++fingerprint)
	{
		threads.emplace_back(
			[&directory, &mapped, fingerprint]
			{
				std::size_t room = 0;
				if (directory.acquire(fingerprint, 100, room) != nullptr)
				{
					++mapped;
				}
			}
		);
	}

	for (auto &thread : threads)
	{
		thread.join();
	}

	CHECK( mapped == 8 );
	CHECK( path.list_files(extension).size() == 4 );
}
