// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/strided_transfer/image_region_loop.hpp>

#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <cstddef>
#include <numeric>
#include <vector>

using namespace vitrio;

namespace
{

image_region_layout make_layout(
	const std::vector<std::size_t> &extents,
	const std::vector<std::ptrdiff_t> &destination_strides,
	const std::vector<std::ptrdiff_t> &source_strides
)
{
	const auto rank = extents.size();
	image_transfer_plan regions(image_transfer_shape(extents, rank, rank));

	return image_region_layout(
		regions,
		make_span(destination_strides),
		make_span(source_strides)
	);
}

struct copy_kernel
{
	void operator()(int *destination, const int *source) const noexcept
	{
		*destination = *source;
	}
};

// Notes the destination of every call, in the order the calls are made.
class recording_kernel
{
public:
	explicit recording_kernel(std::vector<const int*> &visited) noexcept
		: m_visited(&visited)
	{
	}

	void operator()(int *destination, const int *) const
	{
		m_visited->push_back(destination);
	}

private:
	std::vector<const int*> *m_visited;
};

std::vector<int> counting(std::size_t count)
{
	std::vector<int> values(count);
	std::iota(values.begin(), values.end(), 0);
	return values;
}

} // anonymous namespace

TEST_CASE(
	"run_region_loop applies the kernel to every pair of elements",
	"[image_region_loop]"
)
{
	const auto source = counting(12);

	SECTION( "of two contiguous sides" )
	{
		std::vector<int> destination(12, -1);
		const auto layout = make_layout({3, 4}, {4, 1}, {4, 1});

		run_region_loop(
			copy_kernel(),
			layout,
			destination.data(),
			source.data()
		);

		REQUIRE( destination == source );
	}

	SECTION( "of a source that is walked in another order" )
	{
		// The source holds the transposed region: its first axis is the
		// fast one.
		std::vector<int> destination(12, -1);
		const auto layout = make_layout({3, 4}, {4, 1}, {1, 3});

		run_region_loop(
			copy_kernel(),
			layout,
			destination.data(),
			source.data()
		);

		REQUIRE(
			destination ==
			std::vector<int>({0, 3, 6, 9, 1, 4, 7, 10, 2, 5, 8, 11})
		);
	}

	SECTION( "of a destination that skips elements" )
	{
		// Every other element of every other row of a 6 by 8 destination.
		std::vector<int> destination(48, -1);
		const auto layout = make_layout({3, 4}, {16, 2}, {4, 1});

		run_region_loop(
			copy_kernel(),
			layout,
			destination.data(),
			source.data()
		);

		REQUIRE( destination[0] == 0 );
		REQUIRE( destination[2] == 1 );
		REQUIRE( destination[6] == 3 );
		REQUIRE( destination[16] == 4 );
		REQUIRE( destination[38] == 11 );

		// What lies between is left alone.
		REQUIRE( destination[1] == -1 );
		REQUIRE( destination[8] == -1 );
		REQUIRE( destination[47] == -1 );
	}

	SECTION( "of a side that is walked backwards" )
	{
		std::vector<int> destination(12, -1);
		const auto layout = make_layout({12}, {1}, {-1});

		run_region_loop(
			copy_kernel(),
			layout,
			destination.data(),
			source.data() + 11
		);

		REQUIRE(
			destination ==
			std::vector<int>({11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0})
		);
	}

	SECTION( "of three axes that do not merge" )
	{
		std::vector<int> destination(24, -1);
		const auto values = counting(24);

		// The source is laid out with its axes in the opposite order.
		const auto layout = make_layout({2, 3, 4}, {12, 4, 1}, {1, 2, 6});

		run_region_loop(
			copy_kernel(),
			layout,
			destination.data(),
			values.data()
		);

		// Element (i, j, k) comes from i + 2j + 6k.
		REQUIRE( destination[0] == 0 );
		REQUIRE( destination[1] == 6 );
		REQUIRE( destination[4] == 2 );
		REQUIRE( destination[12] == 1 );
		REQUIRE( destination[23] == 1 + 2 * 2 + 6 * 3 );
	}
}

TEST_CASE(
	"run_region_loop visits each element once, in the order of the destination",
	"[image_region_loop]"
)
{
	std::vector<int> destination(12);
	const auto source = counting(12);
	std::vector<const int*> visited;

	SECTION( "whatever order the source is in" )
	{
		const auto layout = make_layout({3, 4}, {4, 1}, {1, 3});

		run_region_loop(
			recording_kernel(visited),
			layout,
			destination.data(),
			source.data()
		);

		REQUIRE( visited.size() == 12 );
		for (std::size_t i = 0; i < visited.size(); ++i)
		{
			REQUIRE( visited[i] == destination.data() + i );
		}
	}

	SECTION( "and when the destination is the transposed side" )
	{
		// The destination has its first axis as the fast one, so that is
		// the one walked innermost.
		const auto layout = make_layout({3, 4}, {1, 3}, {4, 1});

		run_region_loop(
			recording_kernel(visited),
			layout,
			destination.data(),
			source.data()
		);

		REQUIRE( visited.size() == 12 );
		for (std::size_t i = 0; i < visited.size(); ++i)
		{
			REQUIRE( visited[i] == destination.data() + i );
		}
	}
}

TEST_CASE(
	"run_region_loop calls the kernel once for a single element",
	"[image_region_loop]"
)
{
	int destination = -1;
	const int source = 7;
	const auto layout = make_layout({}, {}, {});

	run_region_loop(copy_kernel(), layout, &destination, &source);

	REQUIRE( destination == 7 );
}

TEST_CASE(
	"run_region_loop never calls the kernel for a region of no elements",
	"[image_region_loop]"
)
{
	std::vector<int> destination(12);
	const auto source = counting(12);
	std::vector<const int*> visited;

	SECTION( "when the inner extent is zero" )
	{
		const auto layout = make_layout({3, 0}, {4, 1}, {8, 2});

		run_region_loop(
			recording_kernel(visited),
			layout,
			destination.data(),
			source.data()
		);

		REQUIRE( visited.empty() );
	}

	SECTION( "when an outer extent is zero" )
	{
		const auto layout = make_layout({0, 4}, {4, 1}, {8, 2});

		run_region_loop(
			recording_kernel(visited),
			layout,
			destination.data(),
			source.data()
		);

		REQUIRE( visited.empty() );
	}
}
