// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/raw/planning/image_read_schedule.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <tuning.hpp>

#include <array>
#include <cstddef>
#include <vector>

using namespace vitrio;

namespace
{

// A stack of five 3x4 planes of float32 whose values begin 1024 bytes into
// the file, so one plane is 48 bytes.
const std::vector<std::ptrdiff_t> stack_strides = {12, 4, 1};
constexpr std::size_t values_offset = 1024;
constexpr std::size_t plane_bytes = 3 * 4 * sizeof(float);

// Past the last plane of the fixture, so nothing is clamped unless a case
// asks for it.
constexpr std::size_t whole_file = values_offset + 5 * plane_bytes;

// Small enough that the planes of the fixture do not all land in one page,
// which the page of a machine would make them do.
constexpr std::size_t test_page = 16;

const std::vector<std::size_t> plane = {3, 4};

void add_plane(image_transfer_plan &regions, std::size_t section)
{
	const std::array<std::size_t, 3> file_offset = {section, 0, 0};
	const std::array<std::size_t, 3> array_offset = {0, 0, 0};
	regions.add(make_span(file_offset), make_span(array_offset));
}

// Where the planes named start in the file, in elements and ascending, which
// is how image_region_offsets holds them.
std::vector<std::ptrdiff_t> plane_offsets(
	const std::vector<std::size_t> &sections
)
{
	std::vector<std::ptrdiff_t> offsets;
	for (const auto section : sections)
	{
		offsets.push_back(static_cast<std::ptrdiff_t>(section * 3 * 4));
	}

	return offsets;
}

image_read_policy make_policy(
	std::size_t gap_tolerance,
	std::size_t byte_budget,
	std::size_t alignment = test_page,
	std::size_t alignment_origin = 0
)
{
	return image_read_policy(
		gap_tolerance,
		byte_budget,
		alignment,
		alignment_origin
	);
}

std::size_t span_of(const image_transfer_plan &regions)
{
	return compute_region_span(
		regions,
		make_span(stack_strides),
		numerical_type::float32
	);
}

image_read_schedule schedule(
	const image_transfer_plan &regions,
	const std::vector<std::ptrdiff_t> &file_offsets,
	std::size_t file_size,
	const image_read_policy &policy,
	std::size_t data_offset = values_offset
)
{
	return image_read_schedule(
		regions,
		make_span(stack_strides),
		numerical_type::float32,
		make_span(file_offsets),
		data_offset,
		file_size,
		policy
	);
}

} // anonymous namespace

TEST_CASE( "what one region of a batch spans is its bounding stretch",
	"[image_read_schedule]" )
{

	SECTION( "a whole plane spans the plane" )
	{
		const image_transfer_plan regions(image_transfer_shape(plane, 3, 3));

		REQUIRE( span_of(regions) == plane_bytes );
	}

	SECTION( "a crop spans its bounding box rather than its elements" )
	{
		const std::vector<std::size_t> crop = {2, 2};
		const image_transfer_plan regions(image_transfer_shape(crop, 3, 3));

		// Two rows of two, four elements apart: the first to the last is
		// six elements, not the four it holds.
		REQUIRE( span_of(regions) ==
			6 * sizeof(float) );
	}

	SECTION( "a region of no elements spans nothing" )
	{
		const std::vector<std::size_t> empty = {0, 4};
		const image_transfer_plan regions(image_transfer_shape(empty, 3, 3));

		REQUIRE( span_of(regions) == 0 );
	}
}

TEST_CASE( "a batch is advised as the stretches it reaches",
	"[image_read_schedule]" )
{
	const auto values = values_offset;

	SECTION( "a batch of no region is advised nothing" )
	{
		const image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
		const auto scheduled = schedule(
			regions, plane_offsets({}), whole_file,
			make_policy(0, get_prefetch_byte_budget())
		);

		CHECK( scheduled.get_step_count() == 0 );
		CHECK( scheduled.get_ranges().empty() );
	}

	SECTION( "one region is the stretch it spans" )
	{
		image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
		add_plane(regions, 2);

		const auto scheduled = schedule(
			regions, plane_offsets({2}), whole_file,
			make_policy(0, get_prefetch_byte_budget())
		);

		REQUIRE( scheduled.get_ranges().size() == 1 );
		CHECK( scheduled.get_ranges()[0].get_offset() ==
			values + 2 * plane_bytes );
		CHECK( scheduled.get_ranges()[0].get_size() == plane_bytes );
	}

	SECTION( "consecutive regions merge into one stretch" )
	{
		image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
		add_plane(regions, 1);
		add_plane(regions, 2);
		add_plane(regions, 3);

		const auto scheduled = schedule(
			regions, plane_offsets({1, 2, 3}), whole_file,
			make_policy(0, get_prefetch_byte_budget())
		);

		REQUIRE( scheduled.get_ranges().size() == 1 );
		CHECK( scheduled.get_ranges()[0].get_offset() ==
			values + plane_bytes );
		CHECK( scheduled.get_ranges()[0].get_size() == 3 * plane_bytes );
	}

	SECTION( "regions with a gap between them leave the gap out" )
	{
		image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
		add_plane(regions, 1);
		add_plane(regions, 4);

		const auto scheduled = schedule(
			regions, plane_offsets({1, 4}), whole_file,
			make_policy(0, get_prefetch_byte_budget())
		);

		REQUIRE( scheduled.get_ranges().size() == 2 );
		CHECK( scheduled.get_ranges()[0].get_offset() ==
			values + plane_bytes );
		CHECK( scheduled.get_ranges()[0].get_size() == plane_bytes );
		CHECK( scheduled.get_ranges()[1].get_offset() ==
			values + 4 * plane_bytes );
		CHECK( scheduled.get_ranges()[1].get_size() == plane_bytes );
	}

	SECTION( "a gap within the tolerance is bridged" )
	{
		image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
		add_plane(regions, 0);
		add_plane(regions, 2);

		const auto scheduled = schedule(
			regions, plane_offsets({0, 2}), whole_file,
			make_policy(plane_bytes, get_prefetch_byte_budget())
		);

		REQUIRE( scheduled.get_ranges().size() == 1 );
		CHECK( scheduled.get_ranges()[0].get_offset() == values );
		CHECK( scheduled.get_ranges()[0].get_size() == 3 * plane_bytes );
	}

	SECTION( "a region stated twice is advised once" )
	{
		image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
		add_plane(regions, 3);
		add_plane(regions, 3);

		const auto scheduled = schedule(
			regions, plane_offsets({3, 3}), whole_file,
			make_policy(0, get_prefetch_byte_budget())
		);

		REQUIRE( scheduled.get_ranges().size() == 1 );
		CHECK( scheduled.get_ranges()[0].get_size() == plane_bytes );
	}

	SECTION( "a region of no elements is advised nothing" )
	{
		const std::vector<std::size_t> empty = {0, 4};
		image_transfer_plan regions(image_transfer_shape(empty, 3, 3));
		const std::array<std::size_t, 3> origin = {0, 0, 0};
		regions.add(make_span(origin), make_span(origin));

		const auto scheduled = schedule(
			regions, plane_offsets({0}), whole_file,
			make_policy(0, get_prefetch_byte_budget())
		);

		CHECK( scheduled.get_ranges().empty() );

		// Nothing to ask for, but the region is still one a read moves.
		REQUIRE( scheduled.get_step_count() == 1 );
		CHECK( scheduled.get_step_ranges(0).empty() );
		CHECK( scheduled.get_step_region_count(0) == 1 );
	}
}

TEST_CASE( "every stretch starts on the grid at or before its region, within "
	"the file",
	"[image_read_schedule]" )
{
	const auto values = values_offset;

	image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
	add_plane(regions, 1);

	SECTION( "a stretch starting inside a page grows back to its boundary" )
	{
		// A plane is 48 bytes, so the second one starts 16 bytes into a
		// page of 32.
		const auto scheduled = schedule(
			regions, plane_offsets({1}),
			whole_file,
			make_policy(0, get_prefetch_byte_budget(), 32)
		);

		REQUIRE( scheduled.get_ranges().size() == 1 );
		const auto range = scheduled.get_ranges()[0];
		CHECK( range.get_offset() % 32 == 0 );
		CHECK( range.get_offset() == values + plane_bytes - 16 );
	}

	SECTION( "a stretch is cut short where the file ends" )
	{
		const auto end = values + plane_bytes + 16;
		const auto scheduled = schedule(
			regions, plane_offsets({1}),
			end, make_policy(0, get_prefetch_byte_budget())
		);

		REQUIRE( scheduled.get_ranges().size() == 1 );
		const auto range = scheduled.get_ranges()[0];
		CHECK( range.get_offset() + range.get_size() == end );
	}

	SECTION( "a grid anchored away from the file's start is counted from "
		"there" )
	{
		// Values that begin a byte past the fixture's, on a grid of 32 whose
		// lines lie one byte past a multiple of it: the second plane starts
		// at 1073, 16 bytes past the line at 1057.
		const auto scheduled = schedule(
			regions, plane_offsets({1}),
			whole_file + 1,
			make_policy(0, get_prefetch_byte_budget(), 32, 1),
			values + 1
		);

		REQUIRE( scheduled.get_ranges().size() == 1 );
		CHECK( scheduled.get_ranges()[0].get_offset() == values + 1 + 32 );
		CHECK( scheduled.get_ranges()[0].get_size() == 16 + plane_bytes );
	}

	SECTION( "a grid of the elements of the values leaves a region where it "
		"starts" )
	{
		// The values begin a byte past a multiple of their size, as a format
		// that does not align them may place them.
		const auto scheduled = schedule(
			regions, plane_offsets({1}),
			whole_file + 1,
			make_policy(0, get_prefetch_byte_budget(), sizeof(float), 1),
			values + 1
		);

		REQUIRE( scheduled.get_ranges().size() == 1 );
		CHECK( scheduled.get_ranges()[0].get_offset() ==
			values + 1 + plane_bytes );
		CHECK( scheduled.get_ranges()[0].get_size() == plane_bytes );
	}

	SECTION( "a region starting past the file reaches nothing" )
	{
		const auto scheduled = schedule(
			regions, plane_offsets({1}),
			values, make_policy(0, get_prefetch_byte_budget())
		);

		CHECK( scheduled.get_ranges().empty() );

		// It is left out of the stretches but not out of the steps, which
		// decide what is brought in and not what is moved.
		REQUIRE( scheduled.get_step_count() == 1 );
		CHECK( scheduled.get_step_region_count(0) == 1 );
	}

	SECTION( "a region past the file still belongs to the last step" )
	{
		image_transfer_plan both(image_transfer_shape(plane, 3, 3));
		add_plane(both, 0);
		add_plane(both, 1);

		// Only the first plane is in the file, so the second reaches nothing
		// yet still has to be moved.
		const auto scheduled = schedule(
			both, plane_offsets({0, 1}),
			values + plane_bytes,
			make_policy(0, get_prefetch_byte_budget())
		);

		std::size_t covered = 0;
		for (std::size_t step = 0; step < scheduled.get_step_count(); ++step)
		{
			covered += scheduled.get_step_region_count(step);
		}

		CHECK( covered == 2 );
	}
}

TEST_CASE( "the stretches of a batch are grouped into steps",
	"[image_read_schedule]" )
{

	image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
	add_plane(regions, 0);
	add_plane(regions, 2);
	add_plane(regions, 4);

	const auto offsets = plane_offsets({0, 2, 4});

	SECTION( "a batch within the budget is one step" )
	{
		const auto scheduled = schedule(
			regions, offsets, whole_file,
			make_policy(0, get_prefetch_byte_budget())
		);

		REQUIRE( scheduled.get_step_count() == 1 );
		CHECK( scheduled.get_step_first_region(0) == 0 );
		CHECK( scheduled.get_step_region_count(0) == 3 );
		CHECK( scheduled.get_step_ranges(0).size() == 3 );
	}

	SECTION( "a budget of one stretch is one step each" )
	{
		const auto scheduled = schedule(
			regions, offsets, whole_file,
			make_policy(0, plane_bytes)
		);

		REQUIRE( scheduled.get_step_count() == 3 );
		for (std::size_t step = 0; step < 3; ++step)
		{
			CHECK( scheduled.get_step_first_region(step) == step );
			CHECK( scheduled.get_step_region_count(step) == 1 );
			CHECK( scheduled.get_step_ranges(step).size() == 1 );
		}
	}

	SECTION( "a step holds a region wider than the budget" )
	{
		const auto scheduled = schedule(
			regions, offsets, whole_file,
			make_policy(0, 1)
		);

		REQUIRE( scheduled.get_step_count() == 3 );
		CHECK( scheduled.get_step_region_count(0) == 1 );
		CHECK( scheduled.get_step_ranges(0).size() == 1 );
	}

	SECTION( "the steps cover every region once, in order" )
	{
		const auto scheduled = schedule(
			regions, offsets, whole_file,
			make_policy(0, 2 * plane_bytes)
		);

		std::size_t covered = 0;
		for (std::size_t step = 0; step < scheduled.get_step_count(); ++step)
		{
			CHECK( scheduled.get_step_first_region(step) == covered );
			covered += scheduled.get_step_region_count(step);
		}

		CHECK( covered == 3 );
	}
}

TEST_CASE( "merging never grows a stretch past the budget of a step",
	"[image_read_schedule]" )
{

	SECTION( "a long run of consecutive regions is split into steps" )
	{
		image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
		for (std::size_t section = 0; section < 5; ++section)
		{
			add_plane(regions, section);
		}

		const auto scheduled = schedule(
			regions, plane_offsets({0, 1, 2, 3, 4}),
			whole_file, make_policy(0, 2 * plane_bytes)
		);

		REQUIRE( scheduled.get_ranges().size() == 3 );
		CHECK( scheduled.get_ranges()[0].get_size() == 2 * plane_bytes );
		CHECK( scheduled.get_ranges()[1].get_size() == 2 * plane_bytes );
		CHECK( scheduled.get_ranges()[2].get_size() == plane_bytes );

		REQUIRE( scheduled.get_step_count() == 3 );
		CHECK( scheduled.get_step_first_region(0) == 0 );
		CHECK( scheduled.get_step_region_count(0) == 2 );
		CHECK( scheduled.get_step_first_region(1) == 2 );
		CHECK( scheduled.get_step_region_count(1) == 2 );
		CHECK( scheduled.get_step_first_region(2) == 4 );
		CHECK( scheduled.get_step_region_count(2) == 1 );
	}

	SECTION( "a region wider than the budget takes in no neighbour" )
	{
		image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
		add_plane(regions, 1);
		add_plane(regions, 2);

		const auto scheduled = schedule(
			regions, plane_offsets({1, 2}), whole_file,
			make_policy(0, 1)
		);

		CHECK( scheduled.get_ranges().size() == 2 );
		CHECK( scheduled.get_step_count() == 2 );
	}

	SECTION( "a region stated twice is advised once whatever the budget" )
	{
		image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
		add_plane(regions, 3);
		add_plane(regions, 3);

		const auto scheduled = schedule(
			regions, plane_offsets({3, 3}), whole_file,
			make_policy(0, 1)
		);

		REQUIRE( scheduled.get_ranges().size() == 1 );
		CHECK( scheduled.get_ranges()[0].get_size() == plane_bytes );
		REQUIRE( scheduled.get_step_count() == 1 );
		CHECK( scheduled.get_step_region_count(0) == 2 );
	}
}

TEST_CASE( "a schedule says which regions each stretch holds",
	"[image_read_schedule]" )
{
	image_transfer_plan regions(image_transfer_shape(plane, 3, 3));
	add_plane(regions, 0);
	add_plane(regions, 1);
	add_plane(regions, 3);

	// The first two planes are consecutive and merge, the third does not.
	const auto scheduled = schedule(
		regions, plane_offsets({0, 1, 3}), whole_file,
		make_policy(0, 2 * plane_bytes)
	);

	REQUIRE( scheduled.get_ranges().size() == 2 );
	CHECK( scheduled.get_regions_per_range().size() == 2 );
	CHECK( scheduled.get_regions_per_range()[0] == 2 );
	CHECK( scheduled.get_regions_per_range()[1] == 1 );

	REQUIRE( scheduled.get_step_count() == 2 );
	CHECK( scheduled.get_step_first_range(0) == 0 );
	CHECK( scheduled.get_step_first_range(1) == 1 );
}
