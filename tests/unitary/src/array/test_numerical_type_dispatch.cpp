// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <array/numerical_type_dispatch.hpp>

#include <vitrio/array/numerical_type.hpp>

#include <array/fixed_width_float.hpp>
#include <array/numerical_type_traits.hpp>
#include <array/type_tag.hpp>

#include <complex>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

using namespace vitrio;

namespace
{

// Answers with the numerical_type of the C++ type it is called with, so that
// what goes in can be compared with what comes out.
struct type_reporter
{
	template <typename T>
	numerical_type operator()(type_tag<T>) const noexcept
	{
		return numerical_type_of<T>::value;
	}
};

struct size_reporter
{
	template <typename T>
	std::size_t operator()(type_tag<T>) const noexcept
	{
		return sizeof(T);
	}
};

// Tells whether it was called with one given C++ type.
template <typename Expected>
struct type_checker
{
	template <typename T>
	bool operator()(type_tag<T>) const noexcept
	{
		return std::is_same<T, Expected>::value;
	}
};

} // anonymous namespace

TEST_CASE(
	"dispatch_numerical_type calls the visitor with the type asked for",
	"[numerical_type_dispatch]"
)
{
	SECTION( "a boolean" )
	{
		REQUIRE(
			dispatch_numerical_type(
				type_checker<bool>(),
				numerical_type::boolean
			)
		);
	}

	SECTION( "an integer" )
	{
		REQUIRE(
			dispatch_numerical_type(
				type_checker<std::int8_t>(),
				numerical_type::int8
			)
		);
		REQUIRE(
			dispatch_numerical_type(
				type_checker<std::uint64_t>(),
				numerical_type::uint64
			)
		);
	}

	SECTION( "a floating point number" )
	{
		REQUIRE(
			dispatch_numerical_type(
				type_checker<float16_t>(),
				numerical_type::float16
			)
		);
		REQUIRE(
			dispatch_numerical_type(
				type_checker<double>(),
				numerical_type::float64
			)
		);
	}

	SECTION( "a complex number" )
	{
		REQUIRE(
			dispatch_numerical_type(
				type_checker<std::complex<float16_t>>(),
				numerical_type::complex_float16
			)
		);
		REQUIRE(
			dispatch_numerical_type(
				type_checker<std::complex<float>>(),
				numerical_type::complex_float32
			)
		);
	}
}

TEST_CASE(
	"dispatch_numerical_type maps every type to the C++ type standing for it",
	"[numerical_type_dispatch]"
)
{
	const auto types = {
		numerical_type::boolean,
		numerical_type::int8,
		numerical_type::uint8,
		numerical_type::int16,
		numerical_type::uint16,
		numerical_type::int32,
		numerical_type::uint32,
		numerical_type::int64,
		numerical_type::uint64,
		numerical_type::float16,
		numerical_type::float32,
		numerical_type::float64,
		numerical_type::complex_float16,
		numerical_type::complex_float32,
		numerical_type::complex_float64,
	};

	for (const auto type : types)
	{
		CAPTURE( type );

		// The type comes back, and it is as large as the type says.
		REQUIRE( dispatch_numerical_type(type_reporter(), type) == type );
		REQUIRE(
			dispatch_numerical_type(size_reporter(), type) == get_size(type)
		);
	}
}

TEST_CASE(
	"dispatch_numerical_type returns what the visitor returns",
	"[numerical_type_dispatch]"
)
{
	const auto size = dispatch_numerical_type(
		size_reporter(),
		numerical_type::complex_float64
	);

	REQUIRE( size == 16 );
}

TEST_CASE(
	"dispatch_numerical_type refuses an unknown type",
	"[numerical_type_dispatch]"
)
{
	REQUIRE_THROWS_AS(
		dispatch_numerical_type(type_reporter(), numerical_type::unknown),
		std::invalid_argument
	);
}
