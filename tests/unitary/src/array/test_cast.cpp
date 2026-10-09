// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <array/cast.hpp>

#include <array/fixed_width_float.hpp>

#include <complex>
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
