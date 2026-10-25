// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_chunk_layout.hpp>

#include <limits>
#include <stdexcept>
#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

namespace
{

using strides = std::vector<std::ptrdiff_t>;

strides strides_of(const zarr_chunk_layout &layout)
{
	const auto values = layout.get_strides();
	return strides(values.begin(), values.end());
}

zarr_chunk_layout make_layout(
	std::vector<std::size_t> extents,
	const std::vector<std::size_t> &order,
	numerical_type type = numerical_type::float32
)
{
	return zarr_chunk_layout(
		std::move(extents), make_span(order), type,
		byte_order::little_endian);
}

} // anonymous namespace

TEST_CASE( "zarr_chunk_layout lays a chunk out in the order of memory",
	"[zarr_chunk_layout]" )
{
	SECTION( "C order makes the last axis contiguous" )
	{
		const auto layout = make_layout({2, 3, 4}, {0, 1, 2});
		REQUIRE( strides_of(layout) == strides{12, 4, 1} );
		REQUIRE( layout.get_element_count() == 24 );
		REQUIRE( layout.get_byte_size() == 96 );
	}

	SECTION( "F order makes the first" )
	{
		const auto layout = make_layout({2, 3, 4}, {2, 1, 0});
		REQUIRE( strides_of(layout) == strides{1, 2, 6} );
	}

	SECTION( "any permutation, as a transpose has it" )
	{
		const auto layout = make_layout({2, 3, 4}, {1, 2, 0});
		REQUIRE( strides_of(layout) == strides{1, 8, 2} );
	}

	SECTION( "it keeps the type and the order of its bytes" )
	{
		const zarr_chunk_layout layout(
			{5}, make_span(std::vector<std::size_t>{0}),
			numerical_type::uint16, byte_order::big_endian);
		REQUIRE( layout.get_data_type() == numerical_type::uint16 );
		REQUIRE( layout.get_byte_order() == byte_order::big_endian );
		REQUIRE( layout.get_byte_size() == 10 );
	}

	SECTION( "an order that is no permutation is refused" )
	{
		REQUIRE_THROWS_AS(
			make_layout({2, 3}, {0, 0}), std::invalid_argument);
		REQUIRE_THROWS_AS(
			make_layout({2, 3}, {0}), std::invalid_argument);
		REQUIRE_THROWS_AS(
			make_layout({2, 3}, {0, 2}), std::invalid_argument);
	}

	SECTION( "a chunk too large to count is refused" )
	{
		const auto huge = std::numeric_limits<std::size_t>::max() / 4;
		REQUIRE_THROWS_AS(
			make_layout({huge, 8}, {0, 1}), std::overflow_error);
		REQUIRE_THROWS_AS(
			make_layout({huge / 2}, {0}, numerical_type::complex_float64),
			std::overflow_error);
	}
}
