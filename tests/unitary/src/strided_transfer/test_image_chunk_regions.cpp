// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <strided_transfer/image_chunk_regions.hpp>

#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace vitrio;

namespace
{

using offset = std::vector<std::size_t>;

void add(
	image_transfer_plan &regions,
	const offset &file_offset,
	const offset &array_offset
)
{
	regions.add(make_span(file_offset), make_span(array_offset));
}

offset file_offset_of(const image_transfer_plan &regions, std::size_t index)
{
	const auto values = regions.get_file_offset(index);
	return offset(values.begin(), values.end());
}

offset array_offset_of(const image_transfer_plan &regions, std::size_t index)
{
	const auto values = regions.get_array_offset(index);
	return offset(values.begin(), values.end());
}

offset extents_of(const image_transfer_plan &regions)
{
	const auto values = regions.get_shape().get_extents();
	return offset(values.begin(), values.end());
}

offset chunk_of(const image_chunk_regions &chunks, std::size_t position)
{
	const auto values = chunks.get_chunk(position);
	return offset(values.begin(), values.end());
}

image_chunk_regions resolve(
	const image_transfer_plan &regions,
	const offset &file_extents,
	const offset &chunk_extents
)
{
	return image_chunk_regions(
		regions,
		make_span(file_extents),
		make_span(chunk_extents)
	);
}

} // anonymous namespace

TEST_CASE( "a region within one chunk stays on it",
	"[image_chunk_regions]" )
{
	image_transfer_plan regions(image_transfer_shape({2, 3}, 2, 2));
	add(regions, {12, 21}, {5, 6});

	const auto chunks = resolve(regions, {40, 40}, {10, 10});

	REQUIRE( chunks.get_chunk_count() == 1 );
	REQUIRE( chunk_of(chunks, 0) == offset{1, 2} );
	REQUIRE( chunks.get_plan_count(0) == 1 );

	const auto &piece = chunks.get_regions(0, 0);
	REQUIRE( extents_of(piece) == offset{2, 3} );
	REQUIRE( piece.get_shape().get_file_rank() == 2 );
	REQUIRE( piece.get_shape().get_array_rank() == 2 );
	REQUIRE( piece.get_region_count() == 1 );
	REQUIRE( file_offset_of(piece, 0) == offset{2, 1} );
	REQUIRE( array_offset_of(piece, 0) == offset{5, 6} );
}

TEST_CASE( "a region across chunks is cut into a piece on each",
	"[image_chunk_regions]" )
{
	image_transfer_plan regions(image_transfer_shape({4, 6}, 2, 2));
	add(regions, {8, 7}, {0, 0});

	const auto chunks = resolve(regions, {20, 20}, {10, 10});

	REQUIRE( chunks.get_chunk_count() == 4 );

	SECTION( "the chunks ascend with the last axis fastest" )
	{
		REQUIRE( chunk_of(chunks, 0) == offset{0, 0} );
		REQUIRE( chunk_of(chunks, 1) == offset{0, 1} );
		REQUIRE( chunk_of(chunks, 2) == offset{1, 0} );
		REQUIRE( chunk_of(chunks, 3) == offset{1, 1} );
	}

	SECTION( "each piece is clipped to its chunk and lands where it was" )
	{
		const auto &top_left = chunks.get_regions(0, 0);
		REQUIRE( extents_of(top_left) == offset{2, 3} );
		REQUIRE( file_offset_of(top_left, 0) == offset{8, 7} );
		REQUIRE( array_offset_of(top_left, 0) == offset{0, 0} );

		const auto &top_right = chunks.get_regions(1, 0);
		REQUIRE( extents_of(top_right) == offset{2, 3} );
		REQUIRE( file_offset_of(top_right, 0) == offset{8, 0} );
		REQUIRE( array_offset_of(top_right, 0) == offset{0, 3} );

		const auto &bottom_left = chunks.get_regions(2, 0);
		REQUIRE( extents_of(bottom_left) == offset{2, 3} );
		REQUIRE( file_offset_of(bottom_left, 0) == offset{0, 7} );
		REQUIRE( array_offset_of(bottom_left, 0) == offset{2, 0} );

		const auto &bottom_right = chunks.get_regions(3, 0);
		REQUIRE( extents_of(bottom_right) == offset{2, 3} );
		REQUIRE( file_offset_of(bottom_right, 0) == offset{0, 0} );
		REQUIRE( array_offset_of(bottom_right, 0) == offset{2, 3} );
	}
}

TEST_CASE( "the pieces of a chunk are grouped by their extents",
	"[image_chunk_regions]" )
{
	image_transfer_plan regions(image_transfer_shape({4}, 1, 2));
	add(regions, {0}, {0, 0});
	add(regions, {8}, {1, 0});
	add(regions, {4}, {2, 0});
	add(regions, {9}, {3, 0});

	const auto chunks = resolve(regions, {20}, {10});

	REQUIRE( chunks.get_chunk_count() == 2 );
	REQUIRE( chunk_of(chunks, 0) == offset{0} );
	REQUIRE( chunk_of(chunks, 1) == offset{1} );

	SECTION( "pieces clipped alike share a plan, in the order given" )
	{
		REQUIRE( chunks.get_plan_count(0) == 3 );

		const auto &whole = chunks.get_regions(0, 0);
		REQUIRE( extents_of(whole) == offset{4} );
		REQUIRE( whole.get_region_count() == 2 );
		REQUIRE( file_offset_of(whole, 0) == offset{0} );
		REQUIRE( array_offset_of(whole, 0) == offset{0, 0} );
		REQUIRE( file_offset_of(whole, 1) == offset{4} );
		REQUIRE( array_offset_of(whole, 1) == offset{2, 0} );

		const auto &two = chunks.get_regions(0, 1);
		REQUIRE( extents_of(two) == offset{2} );
		REQUIRE( file_offset_of(two, 0) == offset{8} );
		REQUIRE( array_offset_of(two, 0) == offset{1, 0} );

		const auto &one = chunks.get_regions(0, 2);
		REQUIRE( extents_of(one) == offset{1} );
		REQUIRE( file_offset_of(one, 0) == offset{9} );
		REQUIRE( array_offset_of(one, 0) == offset{3, 0} );
	}

	SECTION( "the rest of each region lands further along the array" )
	{
		REQUIRE( chunks.get_plan_count(1) == 2 );

		const auto &two = chunks.get_regions(1, 0);
		REQUIRE( extents_of(two) == offset{2} );
		REQUIRE( file_offset_of(two, 0) == offset{0} );
		REQUIRE( array_offset_of(two, 0) == offset{1, 2} );

		const auto &three = chunks.get_regions(1, 1);
		REQUIRE( extents_of(three) == offset{3} );
		REQUIRE( file_offset_of(three, 0) == offset{0} );
		REQUIRE( array_offset_of(three, 0) == offset{3, 1} );
	}
}

TEST_CASE( "an axis a region spans one position along is chunked too",
	"[image_chunk_regions]" )
{
	// Images of a stack, the first axis, are chunked two at a time.
	image_transfer_plan regions(image_transfer_shape({3, 3}, 3, 3));
	add(regions, {3, 0, 0}, {0, 0, 0});
	add(regions, {2, 0, 0}, {1, 0, 0});
	add(regions, {0, 1, 1}, {2, 0, 0});

	const auto chunks = resolve(regions, {6, 4, 4}, {2, 4, 4});

	REQUIRE( chunks.get_chunk_count() == 2 );
	REQUIRE( chunk_of(chunks, 0) == offset{0, 0, 0} );
	REQUIRE( chunk_of(chunks, 1) == offset{1, 0, 0} );

	const auto &first = chunks.get_regions(0, 0);
	REQUIRE( extents_of(first) == offset{3, 3} );
	REQUIRE( first.get_shape().get_file_rank() == 3 );
	REQUIRE( file_offset_of(first, 0) == offset{0, 1, 1} );
	REQUIRE( array_offset_of(first, 0) == offset{2, 0, 0} );

	REQUIRE( chunks.get_plan_count(1) == 1 );
	const auto &second = chunks.get_regions(1, 0);
	REQUIRE( second.get_region_count() == 2 );
	REQUIRE( file_offset_of(second, 0) == offset{1, 0, 0} );
	REQUIRE( array_offset_of(second, 0) == offset{0, 0, 0} );
	REQUIRE( file_offset_of(second, 1) == offset{0, 0, 0} );
	REQUIRE( array_offset_of(second, 1) == offset{1, 0, 0} );
}

TEST_CASE( "a region spanning an axis of the stack is cut along it",
	"[image_chunk_regions]" )
{
	// A volume of four planes read into a stack of an array, chunked two
	// planes at a time: the second chunk lands two images further.
	image_transfer_plan regions(image_transfer_shape({4, 2, 2}, 3, 4));
	add(regions, {0, 0, 0}, {1, 0, 0, 0});

	const auto chunks = resolve(regions, {4, 2, 2}, {2, 2, 2});

	REQUIRE( chunks.get_chunk_count() == 2 );

	const auto &first = chunks.get_regions(0, 0);
	REQUIRE( extents_of(first) == offset{2, 2, 2} );
	REQUIRE( array_offset_of(first, 0) == offset{1, 0, 0, 0} );

	const auto &second = chunks.get_regions(1, 0);
	REQUIRE( extents_of(second) == offset{2, 2, 2} );
	REQUIRE( file_offset_of(second, 0) == offset{0, 0, 0} );
	REQUIRE( array_offset_of(second, 0) == offset{1, 2, 0, 0} );
}

TEST_CASE( "a chunk at the edge of the file keeps its whole extents",
	"[image_chunk_regions]" )
{
	image_transfer_plan regions(image_transfer_shape({25}, 1, 1));
	add(regions, {0}, {0});

	const auto chunks = resolve(regions, {25}, {10});

	REQUIRE( chunks.get_chunk_count() == 3 );
	REQUIRE( chunk_of(chunks, 2) == offset{2} );

	const auto &edge = chunks.get_regions(2, 0);
	REQUIRE( extents_of(edge) == offset{5} );
	REQUIRE( file_offset_of(edge, 0) == offset{0} );
	REQUIRE( array_offset_of(edge, 0) == offset{20} );
}

TEST_CASE( "nothing to read reaches no chunk", "[image_chunk_regions]" )
{
	SECTION( "an empty plan" )
	{
		const image_transfer_plan regions(image_transfer_shape({4}, 1, 1));

		const auto chunks = resolve(regions, {20}, {10});

		REQUIRE( chunks.get_chunk_count() == 0 );
	}

	SECTION( "regions of no elements" )
	{
		image_transfer_plan regions(image_transfer_shape({0, 4}, 2, 2));
		add(regions, {3, 3}, {0, 0});

		const auto chunks = resolve(regions, {20, 20}, {10, 10});

		REQUIRE( chunks.get_chunk_count() == 0 );
	}
}

TEST_CASE( "image_chunk_regions refuses what it can not resolve",
	"[image_chunk_regions]" )
{
	image_transfer_plan regions(image_transfer_shape({4}, 2, 2));
	add(regions, {0, 0}, {0, 0});

	SECTION( "extents of the file of another rank" )
	{
		REQUIRE_THROWS_AS(
			resolve(regions, {20}, {10, 10}),
			std::invalid_argument
		);
	}

	SECTION( "extents of a chunk of another rank" )
	{
		REQUIRE_THROWS_AS(
			resolve(regions, {20, 20}, {10}),
			std::invalid_argument
		);
	}

	SECTION( "a chunk of no extent" )
	{
		REQUIRE_THROWS_AS(
			resolve(regions, {20, 20}, {10, 0}),
			std::invalid_argument
		);
	}

	SECTION( "a grid with more chunks than can be counted" )
	{
		const auto huge = std::numeric_limits<std::size_t>::max() / 2;

		REQUIRE_THROWS_AS(
			resolve(regions, {huge, huge}, {1, 1}),
			std::overflow_error
		);
	}
}
