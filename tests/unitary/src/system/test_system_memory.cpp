// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/page_size.hpp>
#include <system/system_memory.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

using namespace vitrio;

TEST_CASE(
	"allocate_system_memory gives memory aligned at a page and filled with "
	"zeros",
	"[system_memory]"
)
{
	const std::size_t size = 3 * get_page_size() + 5;

	const auto memory = allocate_system_memory(size);

	REQUIRE( memory != nullptr );
	CHECK( reinterpret_cast<std::uintptr_t>(memory.get()) %
		get_page_size() == 0 );
	CHECK( std::all_of(
		memory.get(),
		memory.get() + size,
		[](byte value) { return value == 0; }
	) );
}

TEST_CASE(
	"allocate_system_memory gives memory that is written and read back",
	"[system_memory]"
)
{
	const std::size_t size = 2 * get_page_size();
	const auto memory = allocate_system_memory(size);

	for (std::size_t index = 0; index < size; ++index)
	{
		memory.get()[index] = static_cast<byte>(index % 251);
	}

	bool same = true;
	for (std::size_t index = 0; index < size; ++index)
	{
		same = same && memory.get()[index] == static_cast<byte>(index % 251);
	}
	CHECK( same );
}
