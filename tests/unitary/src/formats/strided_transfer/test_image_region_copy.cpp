// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/strided_transfer/image_region_copy.hpp>

#include <vitrio/tests/host_array.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

// Four images of 2 by 2 whose values count up from zero, so image n holds
// 4n, 4n + 1, 4n + 2 and 4n + 3.
const std::vector<std::size_t> stack_extents = {4, 2, 2};
const std::vector<std::size_t> image_extents = {2, 2};

const float untouched = -1.0F;

array make_counting_array(const std::vector<std::size_t> &extents)
{
	auto values =
		make_host_array<float>(extents, numerical_type::float32, 0.0F);
	auto *data = reinterpret_cast<float*>(values.get_data());
	std::iota(data, data + count_elements(extents), 0.0F);

	return values;
}

// Image `indices[i]` of a stack, landing in slot `i` of a batch.
image_transfer_plan images(const std::vector<std::size_t> &indices)
{
	image_transfer_plan plan(image_transfer_shape(image_extents, 3, 3));
	for (std::size_t slot = 0; slot < indices.size(); ++slot)
	{
		const std::array<std::size_t, 3> file_offset = {indices[slot], 0, 0};
		const std::array<std::size_t, 3> array_offset = {slot, 0, 0};
		plan.add(make_span(file_offset), make_span(array_offset));
	}

	return plan;
}

} // anonymous namespace

TEST_CASE(
	"copy_regions copies each region to where the plan places it",
	"[image_region_copy]"
)
{
	const auto source = make_counting_array(stack_extents);
	auto destination =
		make_host_array<float>({3, 2, 2}, numerical_type::float32, untouched);

	copy_regions(
		const_array_ref(source),
		array_ref(destination),
		images({3, 1})
	);

	// The third slot is named by no region and is left as it was.
	CHECK( get_values<float>(destination) == std::vector<float>({
		12, 13, 14, 15,
		4, 5, 6, 7,
		untouched, untouched, untouched, untouched
	}) );
}

TEST_CASE(
	"copy_regions converts to the data type of the destination",
	"[image_region_copy]"
)
{
	const auto source = make_counting_array(stack_extents);
	auto destination =
		make_host_array<double>({1, 2, 2}, numerical_type::float64, -1.0);

	copy_regions(const_array_ref(source), array_ref(destination), images({2}));

	CHECK( get_values<double>(destination) ==
		std::vector<double>({8, 9, 10, 11}) );
}

TEST_CASE(
	"copy_regions copies a region smaller than an image",
	"[image_region_copy]"
)
{
	const auto source = make_counting_array(stack_extents);
	auto destination =
		make_host_array<float>({1, 2}, numerical_type::float32, untouched);

	// The second row of image 2, into an array of one row.
	image_transfer_plan row(image_transfer_shape({1, 2}, 3, 2));
	const std::array<std::size_t, 3> file_offset = {2, 1, 0};
	const std::array<std::size_t, 2> array_offset = {0, 0};
	row.add(make_span(file_offset), make_span(array_offset));

	copy_regions(const_array_ref(source), array_ref(destination), row);

	CHECK( get_values<float>(destination) == std::vector<float>({10, 11}) );
}

TEST_CASE(
	"copy_regions reads a source that starts inside its buffer",
	"[image_region_copy]"
)
{
	// The last two images of the stack, as an array of their own over the
	// same memory.
	auto stack = std::make_shared<std::vector<float>>(16);
	std::iota(stack->begin(), stack->end(), 0.0F);
	const array source(
		as_bytes(make_span(*stack)),
		stack,
		array_descriptor({2, 2, 2}, {4, 2, 1}, 8, numerical_type::float32)
	);
	auto destination =
		make_host_array<float>({1, 2, 2}, numerical_type::float32, untouched);

	copy_regions(const_array_ref(source), array_ref(destination), images({1}));

	CHECK( get_values<float>(destination) ==
		std::vector<float>({12, 13, 14, 15}) );
}

TEST_CASE(
	"copy_regions of an empty plan copies nothing",
	"[image_region_copy]"
)
{
	const auto source = make_counting_array(stack_extents);
	auto destination =
		make_host_array<float>({1, 2, 2}, numerical_type::float32, untouched);

	copy_regions(const_array_ref(source), array_ref(destination), images({}));

	CHECK( get_values<float>(destination) ==
		std::vector<float>(4, untouched) );
}

TEST_CASE(
	"copy_regions refuses what it can not copy",
	"[image_region_copy]"
)
{
	const auto source = make_counting_array(stack_extents);
	auto destination =
		make_host_array<float>({1, 2, 2}, numerical_type::float32, untouched);

	SECTION( "a source that is not initialized" )
	{
		const array empty;

		REQUIRE_THROWS_AS(
			copy_regions(
				const_array_ref(empty),
				array_ref(destination),
				images({0})
			),
			std::invalid_argument
		);
	}

	SECTION( "a destination that is not initialized" )
	{
		array empty;

		REQUIRE_THROWS_AS(
			copy_regions(const_array_ref(source), array_ref(empty), images({})),
			std::invalid_argument
		);
	}

	SECTION( "a region the source does not contain" )
	{
		REQUIRE_THROWS_AS(
			copy_regions(
				const_array_ref(source),
				array_ref(destination),
				images({4})
			),
			std::out_of_range
		);
	}

	SECTION( "a region that does not fit in the destination" )
	{
		REQUIRE_THROWS_AS(
			copy_regions(
				const_array_ref(source),
				array_ref(destination),
				images({0, 1})
			),
			std::out_of_range
		);
	}
}
