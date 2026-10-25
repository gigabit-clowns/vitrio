// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <array/cast.hpp>

#include <array/fixed_width_float.hpp>

#include <cmath>
#include <complex>
#include <cstdint>
#include <type_traits>

using namespace vitrio;

// Values used with exact equality are exactly representable in IEEE half
// precision, so the float16_t round-trips are lossless.

TEST_CASE( "cast converts between scalar types", "[cast]" )
{
	float widened = 0.0f;
	const int source = 5;
	cast(&widened, &source);
	REQUIRE( widened == 5.0f );

	int truncated = 0;
	const double other = 3.9;
	cast(&truncated, &other);
	REQUIRE( truncated == 3 );
}

TEST_CASE( "cast rounds into a float16_t destination", "[cast]" )
{
	const float value = GENERATE(0.5f, 3.75f, -8.0f);
	float16_t destination;
	const float source = value;

	cast(&destination, &source);

	REQUIRE( static_cast<float>(destination) == value );
}

TEST_CASE( "cast rounds a double into a float16_t only once", "[cast]" )
{
	// Past halfway between 1 and the next half precision number, by so
	// little that a float holds only halfway, which then rounds to the even
	// one, 1.
	const double source = 1.0 + std::ldexp(1.0, -11) + std::ldexp(1.0, -40);
	float16_t destination;

	cast(&destination, &source);

	REQUIRE( static_cast<float>(destination) == 1.0F + std::ldexp(1.0F, -10) );
}

TEST_CASE( "cast rounds an integer into a float16_t, ties to even", "[cast]" )
{
	// Above 2048 half precision numbers are two apart, so an odd integer is
	// halfway between two of them.
	const int below = 2049;
	const int above = 2051;
	float16_t destination;

	cast(&destination, &below);
	REQUIRE( static_cast<float>(destination) == 2048.0F );

	cast(&destination, &above);
	REQUIRE( static_cast<float>(destination) == 2052.0F );
}

TEST_CASE( "cast copies a float16_t into a float16_t bit for bit", "[cast]" )
{
	// Negative zero, a NaN with a payload and a signalling NaN: a copy
	// through a float would quiet the last.
	const auto bits = GENERATE(
		std::uint16_t(0x8000), std::uint16_t(0x7E01), std::uint16_t(0x7C01)
	);
	const auto source = float16_t::from_bits(bits);
	auto destination = float16_t::from_bits(0);
	cast(&destination, &source);
	REQUIRE( destination.get_bits() == bits );
}

TEST_CASE( "cast widens from a float16_t source", "[cast]" )
{
	double destination = 0.0;
	const float16_t source(2.5f);

	cast(&destination, &source);

	REQUIRE( destination == 2.5 );
}

TEST_CASE(
	"cast copies a float16_t into a float16_t preserving the value",
	"[cast]"
)
{
	float16_t destination;
	const float16_t source(6.25f);

	cast(&destination, &source);

	REQUIRE( static_cast<float>(destination) == 6.25f );
}

TEST_CASE( "cast converts a scalar into a complex destination", "[cast]" )
{
	std::complex<float> destination;
	const int source = 4;

	cast(&destination, &source);

	REQUIRE( destination.real() == 4.0f );
	REQUIRE( destination.imag() == 0.0f );
}

TEST_CASE(
	"cast converts a float16_t into a complex<float16_t> destination",
	"[cast]"
)
{
	std::complex<float16_t> destination;
	const float16_t source(1.5f);

	cast(&destination, &source);

	REQUIRE( static_cast<float>(destination.real()) == 1.5f );
	REQUIRE( static_cast<float>(destination.imag()) == 0.0f );
}

TEST_CASE( "cast converts between complex types", "[cast]" )
{
	std::complex<double> destination;
	const std::complex<float> source(1.5f, -2.25f);

	cast(&destination, &source);

	REQUIRE( destination.real() == 1.5 );
	REQUIRE( destination.imag() == -2.25 );
}

TEST_CASE(
	"cast widens a complex<float16_t> to a complex<float>",
	"[cast]"
)
{
	std::complex<float> destination;
	const std::complex<float16_t> source(float16_t(3.5f), float16_t(0.5f));

	cast(&destination, &source);

	REQUIRE( destination.real() == 3.5f );
	REQUIRE( destination.imag() == 0.5f );
}

TEST_CASE(
	"cast rounds a complex<float> into a complex<float16_t>",
	"[cast]"
)
{
	std::complex<float16_t> destination;
	const std::complex<float> source(2.5f, -4.75f);

	cast(&destination, &source);

	REQUIRE( static_cast<float>(destination.real()) == 2.5f );
	REQUIRE( static_cast<float>(destination.imag()) == -4.75f );
}

TEST_CASE(
	"cast preserves a complex<float16_t> to a complex<float16_t>",
	"[cast]"
)
{
	std::complex<float16_t> destination;
	const std::complex<float16_t> source(float16_t(1.25f), float16_t(-8.0f));

	cast(&destination, &source);

	REQUIRE( static_cast<float>(destination.real()) == 1.25f );
	REQUIRE( static_cast<float>(destination.imag()) == -8.0f );
}

TEST_CASE( "cast reads a boolean of a file as true unless it is zero",
	"[cast]" )
{
	const std::uint8_t value = GENERATE(0, 1, 2, 255);
	const stored_boolean source = {value};
	const bool expected = value != 0;

	bool boolean = !expected;
	cast(&boolean, &source);
	REQUIRE( boolean == expected );

	int integer = -1;
	cast(&integer, &source);
	REQUIRE( integer == (expected ? 1 : 0) );

	float16_t half;
	cast(&half, &source);
	REQUIRE( static_cast<float>(half) == (expected ? 1.0f : 0.0f) );

	std::complex<double> complex;
	cast(&complex, &source);
	REQUIRE( complex == std::complex<double>(expected ? 1.0 : 0.0, 0.0) );
}

TEST_CASE( "cast stores anything but zero as a true boolean", "[cast]" )
{
	stored_boolean destination = {7};

	const double zero = 0.0;
	cast(&destination, &zero);
	REQUIRE( destination.value == 0 );

	const int negative = -3;
	cast(&destination, &negative);
	REQUIRE( destination.value == 1 );

	const float not_a_number = std::nanf("");
	cast(&destination, &not_a_number);
	REQUIRE( destination.value == 1 );

	const float16_t half_zero(0.0f);
	cast(&destination, &half_zero);
	REQUIRE( destination.value == 0 );

	const float16_t half(0.5f);
	cast(&destination, &half);
	REQUIRE( destination.value == 1 );
}
