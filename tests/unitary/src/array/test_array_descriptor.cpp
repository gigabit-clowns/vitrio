// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array_descriptor.hpp>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace vitrio;

namespace
{

template <typename T>
std::vector<T> to_vector(span<const T> values)
{
	return std::vector<T>(values.begin(), values.end());
}

array_descriptor make_descriptor(
	const std::vector<std::size_t> &extents,
	const std::vector<std::ptrdiff_t> &strides,
	std::ptrdiff_t offset = 0,
	numerical_type data_type = numerical_type::float32
)
{
	return array_descriptor(extents, strides, offset, data_type);
}

} // anonymous namespace

TEST_CASE(
	"a default constructed array_descriptor describes nothing",
	"[array_descriptor]"
)
{
	const array_descriptor descriptor;

	REQUIRE_FALSE( is_initialized(descriptor) );
	REQUIRE( descriptor.get_extents().empty() );
	REQUIRE( descriptor.get_strides().empty() );
	REQUIRE( descriptor.get_offset() == 0 );
	REQUIRE( descriptor.get_data_type() == numerical_type::unknown );
}

TEST_CASE(
	"an array_descriptor reports the components it was constructed from",
	"[array_descriptor]"
)
{
	const auto descriptor =
		make_descriptor({2, 3}, {-6, 2}, 7, numerical_type::int16);

	REQUIRE( is_initialized(descriptor) );
	REQUIRE(
		to_vector(descriptor.get_extents()) == std::vector<std::size_t>({2, 3})
	);
	REQUIRE(
		to_vector(descriptor.get_strides()) ==
		std::vector<std::ptrdiff_t>({-6, 2})
	);
	REQUIRE( descriptor.get_offset() == 7 );
	REQUIRE( descriptor.get_data_type() == numerical_type::int16 );
}

TEST_CASE(
	"an array_descriptor takes over the vectors moved into it",
	"[array_descriptor]"
)
{
	std::vector<std::size_t> extents = {2, 3};
	std::vector<std::ptrdiff_t> strides = {3, 1};
	const auto *extents_data = extents.data();
	const auto *strides_data = strides.data();

	const array_descriptor descriptor(
		std::move(extents),
		std::move(strides),
		0,
		numerical_type::float32
	);

	// Neither vector was copied.
	REQUIRE( descriptor.get_extents().data() == extents_data );
	REQUIRE( descriptor.get_strides().data() == strides_data );
}

TEST_CASE(
	"an array_descriptor may have no extents",
	"[array_descriptor]"
)
{
	const auto descriptor = make_descriptor({}, {});

	// It describes a single element, which is not the same as nothing.
	REQUIRE( is_initialized(descriptor) );
	REQUIRE( descriptor.get_extents().empty() );
}

TEST_CASE(
	"an array_descriptor refuses components that do not agree",
	"[array_descriptor]"
)
{
	SECTION( "fewer strides than extents" )
	{
		REQUIRE_THROWS_AS(
			make_descriptor({2, 3}, {3}),
			std::invalid_argument
		);
	}

	SECTION( "more strides than extents" )
	{
		REQUIRE_THROWS_AS(
			make_descriptor({2}, {3, 1}),
			std::invalid_argument
		);
	}

	SECTION( "an unknown data type" )
	{
		REQUIRE_THROWS_AS(
			make_descriptor({2}, {1}, 0, numerical_type::unknown),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"array_descriptors compare equal when every component matches",
	"[array_descriptor]"
)
{
	const auto descriptor = make_descriptor({2, 3}, {3, 1}, 4);

	REQUIRE( descriptor == make_descriptor({2, 3}, {3, 1}, 4) );
	REQUIRE_FALSE( descriptor != make_descriptor({2, 3}, {3, 1}, 4) );

	SECTION( "and differ with the extents" )
	{
		REQUIRE( descriptor != make_descriptor({2, 2}, {3, 1}, 4) );
	}

	SECTION( "with the strides" )
	{
		REQUIRE( descriptor != make_descriptor({2, 3}, {6, 1}, 4) );
	}

	SECTION( "with the offset" )
	{
		REQUIRE( descriptor != make_descriptor({2, 3}, {3, 1}, 0) );
	}

	SECTION( "with the data type" )
	{
		const auto other =
			make_descriptor({2, 3}, {3, 1}, 4, numerical_type::int32);

		REQUIRE( descriptor != other );
	}

	SECTION( "two that describe nothing are equal" )
	{
		REQUIRE( array_descriptor() == array_descriptor() );
		REQUIRE( descriptor != array_descriptor() );
	}
}

TEST_CASE(
	"an array_descriptor is copied and moved with its components",
	"[array_descriptor]"
)
{
	auto descriptor = make_descriptor({2, 3}, {3, 1}, 4);
	const auto expected = make_descriptor({2, 3}, {3, 1}, 4);

	SECTION( "by copy construction" )
	{
		const array_descriptor copy(descriptor);

		REQUIRE( copy == expected );
		REQUIRE( descriptor == expected );
	}

	SECTION( "by copy assignment" )
	{
		array_descriptor copy;
		copy = descriptor;

		REQUIRE( copy == expected );
	}

	SECTION( "by move construction" )
	{
		const array_descriptor moved(std::move(descriptor));

		REQUIRE( moved == expected );
	}

	SECTION( "by move assignment" )
	{
		array_descriptor moved;
		moved = std::move(descriptor);

		REQUIRE( moved == expected );
	}
}

TEST_CASE(
	"make_contiguous_array_descriptor lays the elements out in row-major order",
	"[array_descriptor]"
)
{
	const std::vector<std::size_t> extents = {2, 3, 4};

	const auto descriptor = make_contiguous_array_descriptor(
		make_span(extents),
		numerical_type::float64
	);

	REQUIRE( to_vector(descriptor.get_extents()) == extents );
	REQUIRE(
		to_vector(descriptor.get_strides()) ==
		std::vector<std::ptrdiff_t>({12, 4, 1})
	);
	REQUIRE( descriptor.get_offset() == 0 );
	REQUIRE( descriptor.get_data_type() == numerical_type::float64 );
}

TEST_CASE(
	"make_contiguous_array_descriptor accepts any number of extents",
	"[array_descriptor]"
)
{
	SECTION( "none, which is a single element" )
	{
		const std::vector<std::size_t> extents;

		const auto descriptor = make_contiguous_array_descriptor(
			make_span(extents),
			numerical_type::float32
		);

		REQUIRE( is_initialized(descriptor) );
		REQUIRE( descriptor.get_extents().empty() );
		REQUIRE( descriptor.get_strides().empty() );
	}

	SECTION( "one" )
	{
		const std::vector<std::size_t> extents = {5};

		const auto descriptor = make_contiguous_array_descriptor(
			make_span(extents),
			numerical_type::float32
		);

		REQUIRE(
			to_vector(descriptor.get_strides()) ==
			std::vector<std::ptrdiff_t>({1})
		);
	}

	SECTION( "one of which is zero" )
	{
		const std::vector<std::size_t> extents = {3, 0, 4};

		const auto descriptor = make_contiguous_array_descriptor(
			make_span(extents),
			numerical_type::float32
		);

		REQUIRE( to_vector(descriptor.get_extents()) == extents );
	}
}

TEST_CASE(
	"make_contiguous_array_descriptor refuses what it cannot describe",
	"[array_descriptor]"
)
{
	SECTION( "an unknown data type" )
	{
		const std::vector<std::size_t> extents = {2, 3};

		REQUIRE_THROWS_AS(
			make_contiguous_array_descriptor(
				make_span(extents),
				numerical_type::unknown
			),
			std::invalid_argument
		);
	}

	SECTION( "more elements than can be addressed" )
	{
		const auto huge = std::numeric_limits<std::size_t>::max() / 2;
		const std::vector<std::size_t> extents = {huge, huge, huge};

		REQUIRE_THROWS_AS(
			make_contiguous_array_descriptor(
				make_span(extents),
				numerical_type::uint8
			),
			std::out_of_range
		);
	}

	SECTION( "an extent that no offset can reach" )
	{
		const auto huge = std::numeric_limits<std::size_t>::max();
		const std::vector<std::size_t> extents = {2, huge};

		REQUIRE_THROWS_AS(
			make_contiguous_array_descriptor(
				make_span(extents),
				numerical_type::uint8
			),
			std::out_of_range
		);
	}
}
