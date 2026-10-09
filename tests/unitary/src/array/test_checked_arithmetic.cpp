// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <array/checked_arithmetic.hpp>

#include <cstddef>
#include <limits>

using namespace vitrio;

namespace
{

constexpr auto lowest = std::numeric_limits<std::ptrdiff_t>::min();
constexpr auto highest = std::numeric_limits<std::ptrdiff_t>::max();

} // anonymous namespace

TEST_CASE(
	"checked_add writes the sum when it fits",
	"[checked_arithmetic]"
)
{
	std::ptrdiff_t result = 0;

	REQUIRE( checked_add(3, 4, result) );
	REQUIRE( result == 7 );

	REQUIRE( checked_add(3, -4, result) );
	REQUIRE( result == -1 );

	SECTION( "up to the limits" )
	{
		REQUIRE( checked_add(highest - 1, 1, result) );
		REQUIRE( result == highest );

		REQUIRE( checked_add(lowest + 1, -1, result) );
		REQUIRE( result == lowest );
	}

	SECTION( "and into one of its operands" )
	{
		std::ptrdiff_t total = 5;

		REQUIRE( checked_add(total, 2, total) );
		REQUIRE( total == 7 );
	}
}

TEST_CASE(
	"checked_add refuses a sum that does not fit",
	"[checked_arithmetic]"
)
{
	std::ptrdiff_t result = 42;

	REQUIRE_FALSE( checked_add(highest, 1, result) );
	REQUIRE_FALSE( checked_add(lowest, -1, result) );

	// The result is left as it was.
	REQUIRE( result == 42 );
}

TEST_CASE(
	"checked_multiply writes the product when it fits",
	"[checked_arithmetic]"
)
{
	std::ptrdiff_t result = 0;

	REQUIRE( checked_multiply(3, 4, result) );
	REQUIRE( result == 12 );

	REQUIRE( checked_multiply(-3, 4, result) );
	REQUIRE( result == -12 );

	REQUIRE( checked_multiply(3, -4, result) );
	REQUIRE( result == -12 );

	REQUIRE( checked_multiply(-3, -4, result) );
	REQUIRE( result == 12 );

	SECTION( "which is zero when either operand is" )
	{
		REQUIRE( checked_multiply(0, highest, result) );
		REQUIRE( result == 0 );

		REQUIRE( checked_multiply(lowest, 0, result) );
		REQUIRE( result == 0 );
	}

	SECTION( "up to the limits" )
	{
		REQUIRE( checked_multiply(highest, 1, result) );
		REQUIRE( result == highest );

		REQUIRE( checked_multiply(lowest, 1, result) );
		REQUIRE( result == lowest );

		REQUIRE( checked_multiply(highest, -1, result) );
		REQUIRE( result == -highest );
	}
}

TEST_CASE(
	"checked_multiply refuses a product that does not fit",
	"[checked_arithmetic]"
)
{
	std::ptrdiff_t result = 42;

	SECTION( "of two positive operands" )
	{
		REQUIRE_FALSE( checked_multiply(highest, 2, result) );
	}

	SECTION( "of a positive and a negative operand" )
	{
		REQUIRE_FALSE( checked_multiply(highest, -2, result) );
		REQUIRE_FALSE( checked_multiply(-2, highest, result) );
	}

	SECTION( "of two negative operands" )
	{
		REQUIRE_FALSE( checked_multiply(lowest, -1, result) );
		REQUIRE_FALSE( checked_multiply(-1, lowest, result) );
	}

	// The result is left as it was.
	REQUIRE( result == 42 );
}
