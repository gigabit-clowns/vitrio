// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/event_counter.hpp>

#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <thread>

using namespace vitrio;

namespace
{

std::uint64_t read_count(const event_counter &counter)
{
	std::uint64_t count = 0;
	REQUIRE( ::read(counter.get_file(), &count, sizeof(count)) == 8 );
	return count;
}

} // anonymous namespace

TEST_CASE(
	"a read of an event_counter returns how often it was signalled, and "
	"resets it",
	"[event_counter]"
)
{
	event_counter counter;
	counter.signal();
	counter.signal();
	CHECK( read_count(counter) == 2 );

	counter.signal();
	CHECK( read_count(counter) == 1 );
}

TEST_CASE(
	"a read of an event_counter waits until it is signalled",
	"[event_counter]"
)
{
	event_counter counter;
	std::thread signaller(
		[&counter]
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			counter.signal();
		}
	);

	CHECK( read_count(counter) == 1 );
	signaller.join();
}
