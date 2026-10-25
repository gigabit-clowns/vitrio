// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/io_uring_file_transfer_port.hpp>

#include "../fixtures/file_transfer_port_contract.hpp"
#include "../fixtures/file_transfer_queue_contract.hpp"
#include "../fixtures/mapped_test_file.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/memory_range.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <thread>
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
		const io_uring_file_transfer_port port(1);
		return true;
	}
	catch (const std::system_error &)
	{
		return false;
	}
}

} // anonymous namespace

TEST_CASE(
	"an io_uring_file_transfer_port nothing wakes keeps the contract of a "
	"file_transfer_queue",
	"[io_uring_file_transfer_port]"
)
{
	if (!opens_a_ring())
	{
		SKIP( "The kernel does not open an io_uring here." );
	}

	// A ring with room for two reads, so that the cases queue more than it
	// holds.
	check_file_transfer_queue_contract(
		"io_uring_file_transfer_port_queue_contract.raw",
		[]
		{
			return std::make_unique<port_file_transfer_queue>(
				std::make_unique<io_uring_file_transfer_port>(2)
			);
		}
	);
}

TEST_CASE(
	"io_uring_file_transfer_port keeps the contract of a file_transfer_port",
	"[io_uring_file_transfer_port]"
)
{
	if (!opens_a_ring())
	{
		SKIP( "The kernel does not open an io_uring here." );
	}

	check_file_transfer_port_contract(
		"io_uring_file_transfer_port_contract.raw",
		[]
		{
			return std::make_unique<io_uring_file_transfer_port>(2);
		}
	);
}

TEST_CASE(
	"an io_uring_file_transfer_port takes any tag, including those it knows "
	"its own read by",
	"[io_uring_file_transfer_port]"
)
{
	if (!opens_a_ring())
	{
		SKIP( "The kernel does not open an io_uring here." );
	}

	const mapped_test_file file("io_uring_file_transfer_port_tags.raw", 1000);
	std::vector<byte> first(10);
	std::vector<byte> second(10);
	const memory_range first_buffer(first.data(), first.size());
	const memory_range second_buffer(second.data(), second.size());

	io_uring_file_transfer_port port(2);
	port.queue_read(file.get_file(), 0, make_span(&first_buffer, 1), 0);
	port.queue_read(
		file.get_file(),
		10,
		make_span(&second_buffer, 1),
		UINT64_MAX
	);

	std::vector<std::uint64_t> tags;
	while (tags.size() < 2)
	{
		const auto completion = port.wait();
		REQUIRE( completion.has_value() );
		CHECK( completion->get_size() == 10 );
		tags.push_back(completion->get_tag());
	}

	CHECK(
		(tags == std::vector<std::uint64_t>{0, UINT64_MAX} ||
		tags == std::vector<std::uint64_t>{UINT64_MAX, 0})
	);
}

TEST_CASE(
	"io_uring_file_transfer_ports open, read and close on many threads at once",
	"[io_uring_file_transfer_port]"
)
{
	if (!opens_a_ring())
	{
		SKIP( "The kernel does not open an io_uring here." );
	}

	// Rings the kernel maps where rings of other threads were, which the
	// thread sanitizer must not take for races.
	const mapped_test_file file(
		"io_uring_file_transfer_port_threads.raw",
		1000
	);
	std::vector<std::thread> threads;
	std::vector<int> read(8, 0);
	for (std::size_t index = 0; index < read.size(); ++index)
	{
		threads.emplace_back(
			[&file, &read, index]
			{
				bool reads = true;
				for (std::size_t round = 0; round < 20; ++round)
				{
					std::vector<byte> bytes(100);
					const memory_range buffer(bytes.data(), bytes.size());
					io_uring_file_transfer_port port(4);
					port.queue_read(
						file.get_file(),
						round,
						make_span(&buffer, 1),
						0
					);
					const auto completion = port.wait();
					reads = reads && completion &&
						completion->get_size() == 100;
				}
				read[index] = reads ? 1 : 0;
			}
		);
	}

	for (auto &thread : threads)
	{
		thread.join();
	}
	CHECK( read == std::vector<int>(8, 1) );
}
