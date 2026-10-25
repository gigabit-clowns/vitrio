// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/const_memory_range.hpp>
#include <vitrio/memory_range.hpp>

#include <array>

using namespace vitrio;

TEST_CASE(
	"a const_memory_range is the stretch it was built from",
	"[const_memory_range]"
)
{
	const std::array<float, 4> values = {};
	const const_memory_range range(values.data() + 1, 3 * sizeof(float));

	CHECK( range.get_address() == static_cast<const void*>(values.data() + 1) );
	CHECK( range.get_size() == 3 * sizeof(float) );
}

TEST_CASE(
	"a const_memory_range covers what the memory_range it is built from "
	"does",
	"[const_memory_range]"
)
{
	std::array<float, 4> values;
	const memory_range writable(values.data(), 2 * sizeof(float));
	const const_memory_range range = writable;

	CHECK( range.get_address() == writable.get_address() );
	CHECK( range.get_size() == writable.get_size() );
}
