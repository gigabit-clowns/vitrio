// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/raw_image_layout.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_descriptor.hpp>

#include <memory/byte_order.hpp>

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace vitrio;

namespace
{

const std::vector<std::size_t> stack_extents = {5, 3, 4};
const std::vector<std::ptrdiff_t> stack_strides = {12, 4, 1};

image_descriptor make_descriptor(
	numerical_type data_type = numerical_type::float32
)
{
	return image_descriptor(make_span(stack_extents), 2, data_type);
}

} // anonymous namespace

TEST_CASE(
	"a raw_image_layout holds what it was constructed with",
	"[raw_image_layout]"
)
{
	const raw_image_layout layout(
		make_descriptor(),
		stack_strides,
		1024,
		byte_order::big_endian
	);

	const auto strides = layout.get_strides();

	CHECK( layout.get_descriptor() == make_descriptor() );
	CHECK( std::vector<std::ptrdiff_t>(strides.begin(), strides.end()) ==
		stack_strides );
	CHECK( layout.get_data_offset() == 1024 );
	CHECK( layout.get_byte_order() == byte_order::big_endian );
}

TEST_CASE(
	"a raw_image_layout reports how many bytes its values occupy",
	"[raw_image_layout]"
)
{
	SECTION( "elements of four bytes" )
	{
		const raw_image_layout layout(
			make_descriptor(),
			stack_strides,
			0,
			byte_order::little_endian
		);

		CHECK( layout.get_data_size() == 60 * 4 );
	}

	SECTION( "elements of one byte" )
	{
		const raw_image_layout layout(
			make_descriptor(numerical_type::uint8),
			stack_strides,
			0,
			byte_order::little_endian
		);

		CHECK( layout.get_data_size() == 60 );
	}
}

TEST_CASE(
	"a raw_image_layout refuses strides that do not have the rank of its "
	"extents",
	"[raw_image_layout]"
)
{
	REQUIRE_THROWS_AS(
		raw_image_layout(
			make_descriptor(),
			std::vector<std::ptrdiff_t>{4, 1},
			0,
			byte_order::little_endian
		),
		std::invalid_argument
	);
}

TEST_CASE(
	"a raw_image_layout tells whether its values can be addressed where "
	"they begin",
	"[raw_image_layout]"
)
{
	SECTION( "an offset that misaligns them is not aligned" )
	{
		CHECK_FALSE( raw_image_layout(
			make_descriptor(),
			stack_strides,
			1026,
			byte_order::little_endian
		).is_aligned() );
	}

	SECTION( "one that keeps them aligned is" )
	{
		CHECK( raw_image_layout(
			make_descriptor(),
			stack_strides,
			1028,
			byte_order::little_endian
		).is_aligned() );
	}

	SECTION( "a narrower element tolerates a smaller multiple" )
	{
		CHECK( raw_image_layout(
			make_descriptor(numerical_type::int8),
			stack_strides,
			1027,
			byte_order::little_endian
		).is_aligned() );
	}
}

TEST_CASE(
	"a raw_image_layout tells whether its values lie one after another",
	"[raw_image_layout]"
)
{
	SECTION( "the strides of a contiguous stack are" )
	{
		CHECK( raw_image_layout(
			make_descriptor(),
			stack_strides,
			1024,
			byte_order::little_endian
		).is_contiguous() );
	}

	SECTION( "axes out of order are not" )
	{
		auto swapped = stack_strides;
		std::swap(swapped[1], swapped[2]);

		CHECK_FALSE( raw_image_layout(
			make_descriptor(),
			swapped,
			1024,
			byte_order::little_endian
		).is_contiguous() );
	}
}
