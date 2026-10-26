// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/raw/planning/image_read_strategy.hpp>

#include <formats/raw/raw_image_layout.hpp>
#include <strided_transfer/image_region_read_walk.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <memory/byte_order.hpp>

#include <cstddef>
#include <vector>

using namespace vitrio;

namespace
{

// An image of 4 rows of 8 float32 values, behind a header of 16 bytes.
const std::vector<std::size_t> image = {4, 8};
const std::vector<std::ptrdiff_t> image_strides = {8, 1};
constexpr std::size_t header = 16;

raw_image_layout make_layout(byte_order order = get_system_byte_order())
{
	return raw_image_layout(
		image_descriptor(make_span(image), 2, numerical_type::float32),
		image_strides,
		header,
		order
	);
}

// A plan of one region of some extents at the origin of the image and of
// an array of those extents.
image_transfer_plan region_of(const std::vector<std::size_t> &extents)
{
	image_transfer_plan regions(image_transfer_shape(extents, 2, 2));
	regions.add(
		make_span(std::vector<std::size_t>{0, 0}),
		make_span(std::vector<std::size_t>{0, 0})
	);
	return regions;
}

image_region_read_walk walk_of(
	const image_transfer_plan &regions,
	const std::vector<std::size_t> &extents
)
{
	const std::vector<std::ptrdiff_t> strides = {
		static_cast<std::ptrdiff_t>(extents[1]), 1
	};
	return image_region_read_walk(
		regions,
		make_span(image),
		make_span(image_strides),
		make_span(extents),
		make_span(strides),
		0
	);
}

byte_order other_byte_order()
{
	return get_system_byte_order() == byte_order::little_endian
		? byte_order::big_endian
		: byte_order::little_endian;
}

} // anonymous namespace

TEST_CASE( "lie_as_they_land tells what can be read straight into place",
	"[image_read_strategy]" )
{
	const auto whole = region_of(image);
	const auto whole_walk = walk_of(whole, image);

	SECTION( "the whole image, as the file holds it" )
	{
		CHECK( lie_as_they_land(
			whole_walk, numerical_type::float32, make_layout()) );
	}

	SECTION( "into another data type" )
	{
		CHECK_FALSE( lie_as_they_land(
			whole_walk, numerical_type::float64, make_layout()) );
	}

	SECTION( "from the other byte order" )
	{
		CHECK_FALSE( lie_as_they_land(
			whole_walk, numerical_type::float32,
			make_layout(other_byte_order())) );
	}

	SECTION( "a patch that cuts across the rows" )
	{
		const std::vector<std::size_t> patch = {2, 3};
		const auto regions = region_of(patch);
		CHECK_FALSE( lie_as_they_land(
			walk_of(regions, patch), numerical_type::float32,
			make_layout()) );
	}
}

TEST_CASE( "schedule_staged_read stages the stretches regions reach",
	"[image_read_strategy]" )
{
	const auto file_size = header + 4 * 8 * sizeof(float);

	SECTION( "the whole image is one stretch, from its first value" )
	{
		const auto regions = region_of(image);
		const auto schedule = schedule_staged_read(
			regions, walk_of(regions, image), make_layout(), file_size,
			1 << 20);

		REQUIRE( schedule.get_ranges().size() == 1 );
		CHECK( schedule.get_ranges()[0].get_offset() == header );
		CHECK( schedule.get_ranges()[0].get_size() == 4 * 8 * sizeof(float) );
		CHECK( schedule.get_regions_per_range()[0] == 1 );
	}

	SECTION( "a patch spans from its first value to its last" )
	{
		const std::vector<std::size_t> patch = {2, 3};
		const auto regions = region_of(patch);
		const auto schedule = schedule_staged_read(
			regions, walk_of(regions, patch), make_layout(), file_size,
			1 << 20);

		REQUIRE( schedule.get_ranges().size() == 1 );
		CHECK( schedule.get_ranges()[0].get_offset() == header );
		CHECK( schedule.get_ranges()[0].get_size() ==
			(8 + 3) * sizeof(float) );
	}
}
