// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/raw/planning/image_region_split.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <cstddef>
#include <vector>

using namespace vitrio;

namespace
{

constexpr std::size_t gap_cap = 128 << 10;
constexpr std::size_t budget = 8 << 20;

// A volume of 300 planes of 1024 by 1024 float32, one after another: a row
// takes a page, and a plane four mebibytes.
const std::vector<std::ptrdiff_t> volume_strides = {1024 * 1024, 1024, 1};

// A micrograph of 4096 by 4096 float32, whose rows lie 16 KiB apart.
const std::vector<std::ptrdiff_t> micrograph_strides = {4096, 1};

// An image 65536 float32 wide, whose rows lie 256 KiB apart: farther than a
// read bridges.
const std::vector<std::ptrdiff_t> wide_strides = {65536, 1};

image_transfer_plan split(
	const image_transfer_plan &regions,
	const std::vector<std::ptrdiff_t> &strides
)
{
	return split_scattered_regions(
		regions,
		make_span(strides),
		numerical_type::float32,
		gap_cap,
		budget
	);
}

std::vector<std::size_t> extents_of(const image_transfer_plan &plan)
{
	const auto extents = plan.get_shape().get_extents();
	return std::vector<std::size_t>(extents.begin(), extents.end());
}

std::vector<std::size_t> file_offset(
	const image_transfer_plan &plan,
	std::size_t region
)
{
	const auto offset = plan.get_file_offset(region);
	return std::vector<std::size_t>(offset.begin(), offset.end());
}

std::vector<std::size_t> array_offset(
	const image_transfer_plan &plan,
	std::size_t region
)
{
	const auto offset = plan.get_array_offset(region);
	return std::vector<std::size_t>(offset.begin(), offset.end());
}

} // anonymous namespace

TEST_CASE(
	"split_scattered_regions leaves regions whose values follow one another "
	"whole, whatever their size",
	"[image_region_split]"
)
{
	// Two whole planes of the volume, 8 MiB together, above the budget.
	image_transfer_plan planes(
		image_transfer_shape(std::vector<std::size_t>{2, 1024, 1024}, 3, 3)
	);
	planes.add(
		make_span(std::vector<std::size_t>{10, 0, 0}),
		make_span(std::vector<std::size_t>{0, 0, 0})
	);

	const auto parts = split(planes, volume_strides);

	REQUIRE( parts.get_region_count() == 1 );
	CHECK( extents_of(parts) == extents_of(planes) );
	CHECK( file_offset(parts, 0) == std::vector<std::size_t>{10, 0, 0} );
}

TEST_CASE(
	"split_scattered_regions keeps the rows of a patch of a micrograph "
	"together, whose gaps a read bridges",
	"[image_region_split]"
)
{
	image_transfer_plan patches(
		image_transfer_shape(std::vector<std::size_t>{128, 128}, 2, 3)
	);
	patches.add(
		make_span(std::vector<std::size_t>{100, 200}),
		make_span(std::vector<std::size_t>{0, 0, 0})
	);
	patches.add(
		make_span(std::vector<std::size_t>{900, 10}),
		make_span(std::vector<std::size_t>{1, 0, 0})
	);

	const auto parts = split(patches, micrograph_strides);

	REQUIRE( parts.get_region_count() == 2 );
	CHECK( extents_of(parts) == std::vector<std::size_t>{128, 128} );
	CHECK( file_offset(parts, 1) == std::vector<std::size_t>{900, 10} );
}

TEST_CASE(
	"split_scattered_regions splits a patch into its rows where they lie "
	"farther apart than a read bridges",
	"[image_region_split]"
)
{
	// Two patches of 4 by 128, landing in the slots of a batch.
	image_transfer_plan patches(
		image_transfer_shape(std::vector<std::size_t>{4, 128}, 2, 3)
	);
	patches.add(
		make_span(std::vector<std::size_t>{100, 200}),
		make_span(std::vector<std::size_t>{0, 0, 0})
	);
	patches.add(
		make_span(std::vector<std::size_t>{900, 10}),
		make_span(std::vector<std::size_t>{1, 0, 0})
	);

	const auto parts = split(patches, wide_strides);

	REQUIRE( parts.get_region_count() == 8 );
	CHECK( extents_of(parts) == std::vector<std::size_t>{1, 128} );
	CHECK( parts.get_shape().get_file_rank() == 2 );
	CHECK( parts.get_shape().get_array_rank() == 3 );
	for (std::size_t row = 0; row < 4; ++row)
	{
		CHECK( file_offset(parts, row) ==
			std::vector<std::size_t>{100 + row, 200} );
		CHECK( array_offset(parts, row) ==
			std::vector<std::size_t>{0, row, 0} );
		CHECK( file_offset(parts, 4 + row) ==
			std::vector<std::size_t>{900 + row, 10} );
		CHECK( array_offset(parts, 4 + row) ==
			std::vector<std::size_t>{1, row, 0} );
	}
}

TEST_CASE(
	"split_scattered_regions splits a slice across the planes of a volume "
	"into a part a plane",
	"[image_region_split]"
)
{
	SECTION( "a slice along y, a row of each plane" )
	{
		image_transfer_plan slice(
			image_transfer_shape(std::vector<std::size_t>{300, 1, 1024}, 3, 4)
		);
		slice.add(
			make_span(std::vector<std::size_t>{0, 17, 0}),
			make_span(std::vector<std::size_t>{2, 0, 0, 0})
		);

		const auto parts = split(slice, volume_strides);

		REQUIRE( parts.get_region_count() == 300 );
		CHECK( extents_of(parts) == std::vector<std::size_t>{1, 1, 1024} );
		CHECK( file_offset(parts, 299) ==
			std::vector<std::size_t>{299, 17, 0} );
		CHECK( array_offset(parts, 299) ==
			std::vector<std::size_t>{2, 299, 0, 0} );
	}

	SECTION( "a slice along x, whose rows lie a page apart, kept within the "
		"budget a plane at a time" )
	{
		image_transfer_plan slice(
			image_transfer_shape(std::vector<std::size_t>{300, 1024, 1}, 3, 3)
		);
		slice.add(
			make_span(std::vector<std::size_t>{0, 0, 40}),
			make_span(std::vector<std::size_t>{0, 0, 0})
		);

		const auto parts = split(slice, volume_strides);

		REQUIRE( parts.get_region_count() == 300 );
		CHECK( extents_of(parts) == std::vector<std::size_t>{1, 1024, 1} );
		CHECK( file_offset(parts, 7) == std::vector<std::size_t>{7, 0, 40} );
		CHECK( array_offset(parts, 7) == std::vector<std::size_t>{7, 0, 0} );
	}
}

TEST_CASE(
	"split_scattered_regions gathers the axes in the order the file lays "
	"them out",
	"[image_region_split]"
)
{
	// A wide image whose first axis is its fastest, as a transposed layout
	// has it: what is a column of the plan is a row of the file.
	const std::vector<std::ptrdiff_t> transposed = {1, 65536};
	image_transfer_plan patch(
		image_transfer_shape(std::vector<std::size_t>{128, 2}, 2, 2)
	);
	patch.add(
		make_span(std::vector<std::size_t>{0, 50}),
		make_span(std::vector<std::size_t>{0, 0})
	);

	const auto parts = split(patch, transposed);

	REQUIRE( parts.get_region_count() == 2 );
	CHECK( extents_of(parts) == std::vector<std::size_t>{128, 1} );
	CHECK( file_offset(parts, 1) == std::vector<std::size_t>{0, 51} );
}
