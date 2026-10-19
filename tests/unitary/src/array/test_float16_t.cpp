// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <array/float16_t.hpp>

#include <memory/reinterpret_bits.hpp>

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>

using namespace vitrio;

// The conversions are checked against what IEEE 754 states, for every
// pattern of bits there is, or for every pair of neighbours, rather than
// against another implementation.

namespace
{

constexpr std::uint32_t pattern_count = 0x10000;

bool is_nan(std::uint16_t bits)
{
	return (bits & 0x7FFFU) > 0x7C00U;
}

// The value a pattern stands for, decoded as IEEE 754 lays out binary16,
// with none of the arithmetic under test. A NaN is left out.
double decode(std::uint16_t bits)
{
	const int exponent = (bits >> 10) & 0x1F;
	const int mantissa = bits & 0x3FF;
	const double sign = (bits & 0x8000U) ? -1.0 : 1.0;

	if (exponent == 0)
	{
		return sign * std::ldexp(mantissa, -24);
	}
	if (exponent == 0x1F)
	{
		return sign * std::numeric_limits<double>::infinity();
	}
	return sign * std::ldexp(1024 + mantissa, exponent - 25);
}

// The bits of the float a pattern stands for. A NaN keeps its sign and its
// payload, in the top of the mantissa.
std::uint32_t float_bits_of(std::uint16_t bits)
{
	if (is_nan(bits))
	{
		return
			(static_cast<std::uint32_t>(bits & 0x8000U) << 16) |
			0x7F800000U |
			(static_cast<std::uint32_t>(bits & 0x3FFU) << 13);
	}
	return reinterpret_bits<std::uint32_t>(static_cast<float>(decode(bits)));
}

// The same for a double.
double double_of(std::uint16_t bits)
{
	if (is_nan(bits))
	{
		return reinterpret_bits<double>(
			(static_cast<std::uint64_t>(bits & 0x8000U) << 48) |
			0x7FF0000000000000ULL |
			(static_cast<std::uint64_t>(bits & 0x3FFU) << 42)
		);
	}
	return decode(bits);
}

std::string describe(std::uint32_t pattern)
{
	std::ostringstream stream;
	stream << "0x" << std::hex << std::setw(4) << std::setfill('0') << pattern;
	return stream.str();
}

// The first pattern a check fails for, or an empty string. One assertion for all
// of them keeps the cases quick under the sanitizers, and a failure still
// names a pattern.
template <typename Check>
std::string first_failure(Check check)
{
	for (std::uint32_t pattern = 0; pattern < pattern_count; ++pattern)
	{
		if (!check(static_cast<std::uint16_t>(pattern)))
		{
			return describe(pattern);
		}
	}
	return std::string();
}

std::uint16_t bits_of(float value)
{
	return float16_t(value).get_bits();
}

std::uint16_t bits_of(double value)
{
	return float16_t(value).get_bits();
}

// Every finite non-negative pattern but the largest has a neighbour above
// it, the next pattern. The largest, 65504, has the value 65536 would
// have, were it not an infinity, above it.
template <typename Float>
Float upper_neighbour_of(std::uint16_t bits)
{
	if (bits == 0x7BFFU)
	{
		return Float(65536);
	}
	return static_cast<Float>(decode(static_cast<std::uint16_t>(bits + 1)));
}

// Whether a value halfway between a pattern and the next one rounds to the
// one of them that is even, and whether a value just to either side of
// halfway rounds to that side. Both signs are checked.
template <typename Float>
bool rounds_to_nearest_even_after(std::uint16_t bits)
{
	if (bits >= 0x7C00U)
	{
		return true;
	}

	for (const std::uint32_t sign : {0x0000U, 0x8000U})
	{
		// The neighbours, the inner one nearer to zero.
		const auto inner_bits = static_cast<std::uint16_t>(bits | sign);
		const auto outer_bits = static_cast<std::uint16_t>((bits + 1) | sign);
		const Float direction = sign ? Float(-1) : Float(1);
		const Float inner = direction * static_cast<Float>(decode(bits));
		const Float outer = direction * upper_neighbour_of<Float>(bits);
		// Exact: either neighbour has eleven bits of mantissa, and their
		// sum has twelve.
		const Float halfway = (inner + outer) / 2;
		const auto even = (bits & 1U) ? outer_bits : inner_bits;

		if (bits_of(halfway) != even ||
			bits_of(std::nextafter(halfway, inner)) != inner_bits ||
			bits_of(std::nextafter(halfway, outer)) != outer_bits)
		{
			return false;
		}
	}
	return true;
}

} // anonymous namespace

TEST_CASE( "float16_t is a trivial type of two bytes", "[float16_t]" )
{
	STATIC_REQUIRE( sizeof(float16_t) == 2 );
	STATIC_REQUIRE( std::is_trivial<float16_t>::value );
	STATIC_REQUIRE( std::is_standard_layout<float16_t>::value );
}

TEST_CASE( "a value-initialized float16_t is positive zero", "[float16_t]" )
{
	REQUIRE( float16_t().get_bits() == 0 );
}

TEST_CASE( "float16_t holds the bits it is made from", "[float16_t]" )
{
	REQUIRE( first_failure([] (std::uint16_t bits)
	{
		return float16_t::from_bits(bits).get_bits() == bits;
	}) == "" );
}

TEST_CASE(
	"float16_t widens every pattern to the float it stands for",
	"[float16_t]"
)
{
	REQUIRE( first_failure([] (std::uint16_t bits)
	{
		const auto widened =
			static_cast<float>(float16_t::from_bits(bits));
		return reinterpret_bits<std::uint32_t>(widened) == float_bits_of(bits);
	}) == "" );
}

TEST_CASE(
	"float16_t narrows every widened pattern back to itself",
	"[float16_t]"
)
{
	// A NaN comes back with its sign and payload, quieted.
	const auto narrowed = [] (std::uint16_t bits)
	{
		return static_cast<std::uint16_t>(is_nan(bits) ? bits | 0x200U : bits);
	};

	SECTION( "from a float" )
	{
		REQUIRE( first_failure([&narrowed] (std::uint16_t bits)
		{
			const auto widened = reinterpret_bits<float>(float_bits_of(bits));
			return bits_of(widened) == narrowed(bits);
		}) == "" );
	}

	SECTION( "from a double" )
	{
		REQUIRE( first_failure([&narrowed] (std::uint16_t bits)
		{
			return bits_of(double_of(bits)) == narrowed(bits);
		}) == "" );
	}
}

TEST_CASE(
	"float16_t rounds to the nearest number, ties to even",
	"[float16_t]"
)
{
	// From zero to the smallest subnormal, across the subnormals, across
	// every binade and from 65504 to where an infinity begins, 65520.
	SECTION( "from a float" )
	{
		REQUIRE( first_failure(rounds_to_nearest_even_after<float>) == "" );
	}

	SECTION( "from a double" )
	{
		REQUIRE( first_failure(rounds_to_nearest_even_after<double>) == "" );
	}
}

TEST_CASE( "float16_t narrows what it cannot hold", "[float16_t]" )
{
	const auto infinity = std::numeric_limits<float>::infinity();
	const auto double_infinity = std::numeric_limits<double>::infinity();

	SECTION( "a value too large becomes an infinity of its sign" )
	{
		REQUIRE( bits_of(65520.0F) == 0x7C00U );
		REQUIRE( bits_of(-65520.0F) == 0xFC00U );
		REQUIRE( bits_of(std::numeric_limits<float>::max()) == 0x7C00U );
		REQUIRE( bits_of(std::numeric_limits<double>::max()) == 0x7C00U );
		REQUIRE( bits_of(-std::numeric_limits<double>::max()) == 0xFC00U );
		REQUIRE( bits_of(infinity) == 0x7C00U );
		REQUIRE( bits_of(-infinity) == 0xFC00U );
		REQUIRE( bits_of(double_infinity) == 0x7C00U );
	}

	SECTION( "a value too small becomes a zero of its sign" )
	{
		REQUIRE( bits_of(0.0F) == 0x0000U );
		REQUIRE( bits_of(-0.0F) == 0x8000U );
		REQUIRE( bits_of(std::ldexp(1.0F, -26)) == 0x0000U );
		REQUIRE( bits_of(std::numeric_limits<float>::denorm_min()) == 0x0000U );
		REQUIRE( bits_of(-std::numeric_limits<float>::denorm_min()) ==
			0x8000U );
		REQUIRE( bits_of(std::numeric_limits<double>::denorm_min()) ==
			0x0000U );
		REQUIRE( bits_of(-std::ldexp(1.0, -40)) == 0x8000U );
	}

	SECTION( "a NaN stays one, quieted, with its sign and its payload" )
	{
		// The payload of the first lies only in the bits that do not fit:
		// it is the quiet bit that keeps it from becoming an infinity.
		REQUIRE( bits_of(reinterpret_bits<float>(0x7F800001U)) == 0x7E00U );
		REQUIRE( bits_of(reinterpret_bits<float>(0xFF800001U)) == 0xFE00U );
		REQUIRE( bits_of(reinterpret_bits<float>(0x7FC00000U | (0x155U << 13))) ==
			0x7F55U );
		REQUIRE( bits_of(reinterpret_bits<double>(0x7FF0000000000001ULL)) ==
			0x7E00U );
		REQUIRE( bits_of(reinterpret_bits<double>(0xFFF8000000000000ULL)) ==
			0xFE00U );
	}
}
