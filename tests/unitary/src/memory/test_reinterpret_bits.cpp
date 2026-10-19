// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <memory/reinterpret_bits.hpp>

#include <cstdint>

using namespace vitrio;

TEST_CASE( "reinterpret_bits reads a float as its bits", "[reinterpret_bits]" )
{
	REQUIRE( reinterpret_bits<std::uint32_t>(1.0F) == 0x3F800000U );
	REQUIRE( reinterpret_bits<std::uint32_t>(-0.0F) == 0x80000000U );
	REQUIRE( reinterpret_bits<std::uint64_t>(-2.0) == 0xC000000000000000ULL );
}

TEST_CASE( "reinterpret_bits reads bits as a float", "[reinterpret_bits]" )
{
	REQUIRE( reinterpret_bits<float>(0x3FC00000U) == 1.5F );
	REQUIRE( reinterpret_bits<double>(0x4004000000000000ULL) == 2.5 );
}

TEST_CASE(
	"reinterpret_bits back and forth gives the bits it started from",
	"[reinterpret_bits]"
)
{
	// A signaling NaN, which no floating point operation may touch on the
	// way.
	const std::uint32_t bits = 0x7F800001U;

	REQUIRE(
		reinterpret_bits<std::uint32_t>(reinterpret_bits<float>(bits)) ==
		bits
	);
}
