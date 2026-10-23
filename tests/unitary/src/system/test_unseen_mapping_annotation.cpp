// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/unseen_mapping_annotation.hpp>

#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <cstddef>
#include <thread>

using namespace vitrio;

namespace
{

constexpr std::size_t mapping_size = 1 << 16;

// Maps memory with a system call of its own, which the thread sanitizer does
// not see, as liburing does. Null if the kernel refuses.
volatile char* map_unseen()
{
	const auto address = ::syscall(
		SYS_mmap,
		nullptr,
		mapping_size,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1,
		0
	);
	if (address == -1)
	{
		return nullptr;
	}
	return reinterpret_cast<volatile char*>(address);
}

void unmap_unseen(volatile char *address)
{
	::syscall(SYS_munmap, const_cast<char*>(address), mapping_size);
}

} // anonymous namespace

TEST_CASE(
	"memory mapped where another thread unmapped, unseen, is not taken for "
	"a race once announced",
	"[unseen_mapping_annotation]"
)
{
	// The threads are ordered by an atomic flag read with relaxed order,
	// which orders nothing for the sanitizer: only the announcements do.
	std::atomic<bool> unmapped(false);
	volatile char *first_address = nullptr;
	volatile char *second_address = nullptr;

	std::thread first(
		[&]
		{
			auto *mapping = map_unseen();
			if (mapping != nullptr)
			{
				announce_unseen_mapping();
				mapping[0] = 1;
				first_address = mapping;

				announce_unseen_unmapping();
				unmap_unseen(mapping);
			}
			unmapped.store(true, std::memory_order_relaxed);
		}
	);

	std::thread second(
		[&]
		{
			while (!unmapped.load(std::memory_order_relaxed))
			{
				std::this_thread::yield();
			}

			auto *mapping = map_unseen();
			if (mapping != nullptr)
			{
				announce_unseen_mapping();
				mapping[0] = 2;
				second_address = mapping;

				announce_unseen_unmapping();
				unmap_unseen(mapping);
			}
		}
	);

	first.join();
	second.join();

	// The kernel usually gives the second mapping the address of the first,
	// which is the case that matters; when it does not, nothing was at risk.
	CHECK( first_address != nullptr );
	CHECK( second_address != nullptr );
}
