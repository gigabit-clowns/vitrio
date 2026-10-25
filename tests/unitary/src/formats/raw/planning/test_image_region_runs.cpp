// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/raw/planning/image_region_runs.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <cstddef>
#include <vector>

using namespace vitrio;

namespace
{

// A stack of five 3x4 planes of float32, laid out one after another.
const std::vector<std::ptrdiff_t> stack_strides = {12, 4, 1};

image_transfer_plan make_regions(const std::vector<std::size_t> &extents)
{
	return image_transfer_plan(image_transfer_shape(extents, 3, 3));
}

std::vector<std::size_t> offsets_of(const image_region_runs &runs)
{
	const auto offsets = runs.get_run_offsets();
	return std::vector<std::size_t>(offsets.begin(), offsets.end());
}

} // anonymous namespace

TEST_CASE(
	"a region whose elements follow one another in the file is one run",
	"[image_region_runs]"
)
{
	SECTION( "a whole plane" )
	{
		const image_region_runs runs(
			make_regions({3, 4}),
			make_span(stack_strides),
			numerical_type::float32
		);

		CHECK( runs.get_run_size() == 3 * 4 * sizeof(float) );
		CHECK( offsets_of(runs) == std::vector<std::size_t>{0} );
	}

	SECTION( "several whole planes" )
	{
		const image_region_runs runs(
			make_regions({2, 3, 4}),
			make_span(stack_strides),
			numerical_type::float32
		);

		CHECK( runs.get_run_size() == 2 * 3 * 4 * sizeof(float) );
		CHECK( offsets_of(runs) == std::vector<std::size_t>{0} );
	}

	SECTION( "whole rows of a plane" )
	{
		const image_region_runs runs(
			make_regions({2, 4}),
			make_span(stack_strides),
			numerical_type::float32
		);

		CHECK( runs.get_run_size() == 2 * 4 * sizeof(float) );
		CHECK( offsets_of(runs) == std::vector<std::size_t>{0} );
	}
}

TEST_CASE(
	"a region that cuts across rows is a run for each row it takes",
	"[image_region_runs]"
)
{
	SECTION( "a patch of a plane" )
	{
		const image_region_runs runs(
			make_regions({3, 2}),
			make_span(stack_strides),
			numerical_type::float32
		);

		CHECK( runs.get_run_size() == 2 * sizeof(float) );
		CHECK(
			offsets_of(runs) == std::vector<std::size_t>{
				0, 4 * sizeof(float), 8 * sizeof(float)
			}
		);
	}

	SECTION( "a box across planes" )
	{
		const image_region_runs runs(
			make_regions({2, 2, 3}),
			make_span(stack_strides),
			numerical_type::float32
		);

		CHECK( runs.get_run_size() == 3 * sizeof(float) );
		CHECK(
			offsets_of(runs) == std::vector<std::size_t>{
				0, 4 * sizeof(float), 12 * sizeof(float), 16 * sizeof(float)
			}
		);
	}

	SECTION( "a column of a plane is a run for each element" )
	{
		const image_region_runs runs(
			make_regions({3, 1}),
			make_span(stack_strides),
			numerical_type::float32
		);

		CHECK( runs.get_run_size() == sizeof(float) );
		CHECK(
			offsets_of(runs) == std::vector<std::size_t>{
				0, 4 * sizeof(float), 8 * sizeof(float)
			}
		);
	}
}

TEST_CASE(
	"the runs follow the strides of the file, whatever order it lays its "
	"axes in",
	"[image_region_runs]"
)
{
	// The file lays its first axis out fastest: a 3x4 plane by columns.
	const std::vector<std::ptrdiff_t> column_strides = {1, 3};
	const image_transfer_plan regions(
		image_transfer_shape(std::vector<std::size_t>{2, 4}, 2, 2)
	);

	const image_region_runs runs(
		regions,
		make_span(column_strides),
		numerical_type::int16
	);

	CHECK( runs.get_run_size() == 2 * 2 );
	CHECK(
		offsets_of(runs) == std::vector<std::size_t>{0, 6, 12, 18}
	);
}

TEST_CASE(
	"a region of no element lays no run",
	"[image_region_runs]"
)
{
	const image_region_runs runs(
		make_regions({0, 4}),
		make_span(stack_strides),
		numerical_type::float32
	);

	CHECK( runs.get_run_size() == 0 );
	CHECK( runs.get_run_offsets().empty() );
}
