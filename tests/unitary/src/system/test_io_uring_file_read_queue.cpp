// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/io_uring_file_read_queue.hpp>

#include "../fixtures/file_read_queue_contract.hpp"

#include <cstddef>
#include <memory>
#include <system_error>

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
