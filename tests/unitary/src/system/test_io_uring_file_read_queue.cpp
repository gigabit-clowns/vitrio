// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/io_uring_file_read_queue.hpp>

#include "../fixtures/file_read_queue_contract.hpp"
#include "../fixtures/mapped_test_file.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/memory_range.hpp>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <vector>

using namespace vitrio;

namespace
{

// Whether the kernel opens a ring for this process, which one without
// io_uring, one that has it turned off, and a sandbox that filters its calls
// each refuse.
bool opens_a_ring()
{
	try
	{
		const io_uring_file_read_queue queue(1);
		return true;
	}
	catch (const std::system_error &)
	{
		return false;
	}
}

// Whether reads of four stretches of the file through the queue each get
// their bytes, waited for together.
bool reads_through(
	file_read_queue &queue,
	const mapped_test_file &file,
	std::size_t first
)
{
	constexpr std::size_t size = 100;
	std::vector<std::vector<byte>> reads(4, std::vector<byte>(size));
	for (std::uint64_t tag = 0; tag < reads.size(); ++tag)
	{
		const memory_range buffer(reads[tag].data(), size);
		queue.queue(
			file.get_file(),
			first + tag * size,
			make_span(&buffer, 1),
			tag
		);
	}

	bool read = true;
	for (std::size_t count = 0; count < reads.size(); ++count)
	{
		const auto completion = queue.wait();
		read = read && !completion.get_error() &&
			completion.get_tag() < reads.size() &&
			completion.get_size() == size;
	}

	const auto bytes = file.get_bytes();
	for (std::size_t tag = 0; tag < reads.size(); ++tag)
	{
		const auto at = bytes.begin() + first + tag * size;
		read = read && std::equal(reads[tag].begin(), reads[tag].end(), at);
	}
	return read;
}

} // anonymous namespace

TEST_CASE(
	"io_uring_file_read_queue keeps the contract of a file_read_queue",
	"[io_uring_file_read_queue]"
)
{
	if (!opens_a_ring())
	{
		SKIP( "The kernel does not open an io_uring here." );
	}

	// A ring with room for two reads, so that the cases queue more than it
	// holds.
	check_file_read_queue_contract(
		"io_uring_file_read_queue_contract.raw",
		[]
		{
			return std::make_unique<io_uring_file_read_queue>(2);
		}
	);
}

TEST_CASE(
	"an io_uring_file_read_queue the child of a fork inherits reads in both "
	"processes",
	"[io_uring_file_read_queue]"
)
{
	if (!opens_a_ring())
	{
		SKIP( "The kernel does not open an io_uring here." );
	}

	const mapped_test_file file("io_uring_file_read_queue_fork.raw", 10000);
	io_uring_file_read_queue queue(4);
	REQUIRE( reads_through(queue, file, 0) );

	// Both processes read at once, time and again: sharing a ring, each would
	// take completions of the other's reads.
	const auto child = ::fork();
	REQUIRE( child >= 0 );

	bool read = true;
	for (std::size_t round = 0; round < 200; ++round)
	{
		read = read && reads_through(queue, file, round * 37 % 9000);
	}

	if (child == 0)
	{
		::_exit(read ? 0 : 1);
	}

	int status = 0;
	REQUIRE( ::waitpid(child, &status, 0) == child );
	CHECK( read );
	CHECK( WIFEXITED(status) );
	CHECK( WEXITSTATUS(status) == 0 );
}
