// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/memory_range.hpp>

#include <array>

using namespace vitrio;

TEST_CASE(
	"a memory_range is the stretch it was built from",
	"[memory_range]"
)
{
	std::array<float, 4> values;
	const memory_range range(values.data() + 1, 3 * sizeof(float));

	CHECK( range.get_address() == static_cast<void*>(values.data() + 1) );
	CHECK( range.get_size() == 3 * sizeof(float) );
}
