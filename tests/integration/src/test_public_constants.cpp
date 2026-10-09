// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/interned_key_list.hpp>

#include <cstddef>
#include <limits>

using namespace vitrio;

namespace
{

// Binding a constant to a reference is what makes C++14 ask the shared
// library for its definition.
std::size_t read_through_reference(const std::size_t &value) noexcept
{
	return value;
}

} // anonymous namespace

TEST_CASE(
	"The constants of the public classes are defined by the shared library",
	"[public_constants]"
)
{
	const auto highest = std::numeric_limits<std::size_t>::max();

	REQUIRE(
		read_through_reference(image_location::no_stack_index) == highest
	);
	REQUIRE( read_through_reference(interned_key_list::no_key) == highest );
	REQUIRE(
		read_through_reference(image_file_probe::max_leading_bytes) == 4096
	);
}
