// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/strided_transfer/image_region_layout.hpp>

#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <cstddef>
#include <vector>

using namespace vitrio;

namespace
{

image_transfer_plan one_region(
	const std::vector<std::size_t> &extents,
	std::size_t file_rank,
	std::size_t array_rank
)
{
	image_transfer_plan regions(
		image_transfer_shape(extents, file_rank, array_rank)
	);
	regions.add(
		make_span(std::vector<std::size_t>(file_rank, 0)),
		make_span(std::vector<std::size_t>(array_rank, 0))
	);

	return regions;
}

} // anonymous namespace

TEST_CASE(
	"an image_region_layout axis reports the components it was given",
	"[image_region_layout]"
)
{
	const image_region_layout::axis axis(5, -3, 7);

	REQUIRE( axis.get_extent() == 5 );
	REQUIRE( axis.get_destination_stride() == -3 );
	REQUIRE( axis.get_source_stride() == 7 );
}

TEST_CASE(
	"the destination decides the order the axes are walked in",
	"[image_region_layout]"
)
{
	// Neither side is contiguous in the other's order, so the two disagree
	// about which axis should be innermost and nothing merges. That is what
	// makes the choice observable.
	const std::vector<std::size_t> extents = {4, 8};
	const std::vector<std::ptrdiff_t> row_major = {8, 1};
	const std::vector<std::ptrdiff_t> column_major = {1, 4};
	const auto regions = one_region(extents, 2, 2);

	const image_region_layout rows_first(
		regions,
		make_span(row_major),
		make_span(column_major)
	);
	const image_region_layout columns_first(
		regions,
		make_span(column_major),
		make_span(row_major)
	);

	SECTION( "the destination is the side walked with a unit stride" )
	{
		REQUIRE( rows_first.get_axis(0).get_destination_stride() == 1 );
		REQUIRE( rows_first.get_axis(0).get_source_stride() != 1 );

		REQUIRE( columns_first.get_axis(0).get_destination_stride() == 1 );
		REQUIRE( columns_first.get_axis(0).get_source_stride() != 1 );
	}

	SECTION( "each axis keeps the extent and the strides it came with" )
	{
		REQUIRE( rows_first.get_axis_count() == 2 );
		REQUIRE( rows_first.get_axis(0).get_extent() == 8 );
		REQUIRE( rows_first.get_axis(0).get_source_stride() == 4 );
		REQUIRE( rows_first.get_axis(1).get_extent() == 4 );
		REQUIRE( rows_first.get_axis(1).get_destination_stride() == 8 );
		REQUIRE( rows_first.get_axis(1).get_source_stride() == 1 );

		REQUIRE( columns_first.get_axis_count() == 2 );
		REQUIRE( columns_first.get_axis(0).get_extent() == 4 );
		REQUIRE( columns_first.get_axis(1).get_extent() == 8 );
	}

	SECTION( "both orders visit the same number of elements" )
	{
		REQUIRE( rows_first.compute_element_count() == 32 );
		REQUIRE( columns_first.compute_element_count() == 32 );
	}
}

TEST_CASE(
	"the source decides the order where the destination does not",
	"[image_region_layout]"
)
{
	// A destination stride of zero says nothing about where its axis
	// belongs, so the source is asked. Its strides are not contiguous, so
	// that the two axes stay apart and their order can be seen.
	const std::vector<std::size_t> extents = {4, 8};
	const std::vector<std::ptrdiff_t> destination = {0, 0};
	const std::vector<std::ptrdiff_t> source = {1, 5};
	const auto regions = one_region(extents, 2, 2);

	const image_region_layout layout(
		regions,
		make_span(destination),
		make_span(source)
	);

	REQUIRE( layout.get_axis(0).get_source_stride() == 1 );
	REQUIRE( layout.get_axis(0).get_extent() == 4 );
}

TEST_CASE(
	"the direction of a stride does not change where its axis belongs",
	"[image_region_layout]"
)
{
	const std::vector<std::size_t> extents = {4, 8};
	const std::vector<std::ptrdiff_t> destination = {-1, 4};
	const std::vector<std::ptrdiff_t> source = {8, 1};
	const auto regions = one_region(extents, 2, 2);

	const image_region_layout layout(
		regions,
		make_span(destination),
		make_span(source)
	);

	REQUIRE( layout.get_axis(0).get_destination_stride() == -1 );
	REQUIRE( layout.get_axis(1).get_destination_stride() == 4 );
}

TEST_CASE(
	"axes that both sides walk contiguously are merged into one",
	"[image_region_layout]"
)
{
	const std::vector<std::size_t> extents = {2, 3, 4};
	const std::vector<std::ptrdiff_t> contiguous = {12, 4, 1};
	const auto regions = one_region(extents, 3, 3);

	SECTION( "all of them, when both sides are contiguous throughout" )
	{
		const image_region_layout layout(
			regions,
			make_span(contiguous),
			make_span(contiguous)
		);

		REQUIRE( layout.get_axis_count() == 1 );
		REQUIRE( layout.get_axis(0).get_extent() == 24 );
		REQUIRE( layout.get_axis(0).get_destination_stride() == 1 );
		REQUIRE( layout.get_axis(0).get_source_stride() == 1 );
	}

	SECTION( "only as far as both sides stay contiguous" )
	{
		// The source skips every other plane, so the outermost axis is not
		// a continuation of the two inside it.
		const std::vector<std::ptrdiff_t> skipping = {24, 4, 1};

		const image_region_layout layout(
			regions,
			make_span(contiguous),
			make_span(skipping)
		);

		REQUIRE( layout.get_axis_count() == 2 );
		REQUIRE( layout.get_axis(0).get_extent() == 12 );
		REQUIRE( layout.get_axis(1).get_extent() == 2 );
		REQUIRE( layout.get_axis(1).get_destination_stride() == 12 );
		REQUIRE( layout.get_axis(1).get_source_stride() == 24 );
		REQUIRE( layout.compute_element_count() == 24 );
	}

	SECTION( "not at all, when one side is not contiguous anywhere" )
	{
		const std::vector<std::ptrdiff_t> spread = {100, 10, 2};

		const image_region_layout layout(
			regions,
			make_span(contiguous),
			make_span(spread)
		);

		REQUIRE( layout.get_axis_count() == 3 );
		REQUIRE( layout.compute_element_count() == 24 );
	}
}

TEST_CASE(
	"an axis of one element is merged whatever its strides are",
	"[image_region_layout]"
)
{
	const std::vector<std::ptrdiff_t> destination = {40, 4, 1};
	const std::vector<std::ptrdiff_t> source = {700, 90, 5};

	SECTION( "when it is the innermost one" )
	{
		const auto regions = one_region({3, 4, 1}, 3, 3);

		const image_region_layout layout(
			regions,
			make_span(destination),
			make_span(source)
		);

		REQUIRE( layout.get_axis_count() == 2 );
		REQUIRE( layout.get_axis(0).get_extent() == 4 );
		REQUIRE( layout.get_axis(0).get_destination_stride() == 4 );
		REQUIRE( layout.get_axis(0).get_source_stride() == 90 );
		REQUIRE( layout.get_axis(1).get_extent() == 3 );
	}

	SECTION( "when it is an outer one" )
	{
		const auto regions = one_region({1, 3, 4}, 3, 3);

		const image_region_layout layout(
			regions,
			make_span(destination),
			make_span(source)
		);

		REQUIRE( layout.get_axis_count() == 2 );
		REQUIRE( layout.get_axis(0).get_extent() == 4 );
		REQUIRE( layout.get_axis(1).get_extent() == 3 );
		REQUIRE( layout.compute_element_count() == 12 );
	}
}

TEST_CASE(
	"a side deeper than the region contributes only its inner axes",
	"[image_region_layout]"
)
{
	// The extents of a plan cover the trailing axes of a side. The leading
	// ones are where a region starts, which is a pointer offset rather than
	// something the space is walked along, so their strides are left out.
	const std::vector<std::size_t> extents = {3, 4};
	const std::vector<std::ptrdiff_t> file_strides = {12, 4, 1};
	const std::vector<std::ptrdiff_t> array_strides = {4, 1};
	const auto regions = one_region(extents, 3, 2);

	const image_region_layout layout(
		regions,
		make_span(array_strides),
		make_span(file_strides)
	);

	// Both sides are contiguous over the region here, so its two axes merge
	// into one of twelve elements.
	REQUIRE( layout.get_axis_count() == 1 );
	REQUIRE( layout.compute_element_count() == 12 );
	REQUIRE( layout.get_axis(0).get_destination_stride() == 1 );
	REQUIRE( layout.get_axis(0).get_source_stride() == 1 );
}

TEST_CASE(
	"a region that is a single element has no axis to walk",
	"[image_region_layout]"
)
{
	const std::vector<std::ptrdiff_t> strides = {4, 1};
	const auto regions = one_region({}, 2, 2);

	const image_region_layout layout(
		regions,
		make_span(strides),
		make_span(strides)
	);

	REQUIRE( layout.get_axis_count() == 0 );
	REQUIRE( layout.compute_element_count() == 1 );
}

TEST_CASE(
	"a region of no elements yields a space that holds none",
	"[image_region_layout]"
)
{
	const std::vector<std::size_t> extents = {0, 4};
	const std::vector<std::ptrdiff_t> strides = {4, 1};
	const auto regions = one_region(extents, 2, 2);

	const image_region_layout layout(
		regions,
		make_span(strides),
		make_span(strides)
	);

	REQUIRE( layout.compute_element_count() == 0 );
}
