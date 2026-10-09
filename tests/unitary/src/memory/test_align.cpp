// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <memory/align.hpp>

#include <cstddef>
#include <cstdint>
#include <tuple>

using namespace vitrio;

TEST_CASE(
	"is_aligned tells whether an address is a multiple of an alignment",
	"[align]"
)
{
	std::uintptr_t address;
	std::size_t alignment;
	bool expected;
	std::tie(address, alignment, expected) = GENERATE(
		table<std::uintptr_t, std::size_t, bool>({
			{0xA1010100, 0x10,   true},
			{0xA1010100, 0x100,  true},
			{0xA1010100, 0x1000, false},
			{0xA1010111, 0x10,   false},
			{0xA1010111, 0x100,  false},
			{0xA1010111, 0x1000, false},
			{0x000000FF, 0x10,   false},
			{0x00001001, 0x100,  false}
		})
	);

	void *pointer = reinterpret_cast<void*>(address);

	REQUIRE( is_aligned(address, alignment) == expected );
	REQUIRE( is_aligned(pointer, alignment) == expected );
}

TEST_CASE(
	"align_ceil rounds an address up to a multiple of an alignment",
	"[align]"
)
{
	std::uintptr_t address;
	std::size_t alignment;
	std::uintptr_t expected;
	std::tie(address, alignment, expected) = GENERATE(
		table<std::uintptr_t, std::size_t, std::uintptr_t>({
			{0xA1010100, 0x10,   0xA1010100},
			{0xA1010100, 0x100,  0xA1010100},
			{0xA1010100, 0x1000, 0xA1011000},
			{0xA1010111, 0x10,   0xA1010120},
			{0xA1010111, 0x100,  0xA1010200},
			{0xA1010111, 0x1000, 0xA1011000},
			{0x000000FF, 0x10,   0x00000100},
			{0x00001001, 0x100,  0x00001100}
		})
	);

	REQUIRE( align_ceil(address, alignment) == expected );
}
