// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/strided_transfer/image_region_loop.hpp>

#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <cstddef>
#include <numeric>
#include <type_traits>
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

// Runs one region with the inner loop the dispatch chooses for its layout, as
// a transfer does.
template <typename Kernel>
void run_dispatched_region_loop(
	const Kernel &kernel,
	const image_region_layout &layout,
	int *destination,
	const int *source
)
{
	dispatch_region_inner_strides(
		layout,
		[&] (auto destination_stride, auto source_stride)
		{
			run_region_loop(
				kernel,
				layout,
				destination_stride,
				source_stride,
				destination,
				source
			);
		}
	);
}

// What the dispatch handed over: whether each stride came as a constant, and
// its value either way.
struct dispatched_strides
{
	int calls = 0;
	bool destination_is_unit = false;
	bool source_is_unit = false;
	std::ptrdiff_t destination_stride = 0;
	std::ptrdiff_t source_stride = 0;
};

dispatched_strides dispatch(const image_region_layout &layout)
{
	dispatched_strides result;
	dispatch_region_inner_strides(
		layout,
		[&result] (auto destination_stride, auto source_stride)
		{
			++result.calls;
			result.destination_is_unit = std::is_same<
				decltype(destination_stride),
				region_unit_stride
			>::value;
			result.source_is_unit = std::is_same<
				decltype(source_stride),
				region_unit_stride
			>::value;
			result.destination_stride = destination_stride;
			result.source_stride = source_stride;
		}
	);
	return result;
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

		run_dispatched_region_loop(
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

		run_dispatched_region_loop(
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

		run_dispatched_region_loop(
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

	SECTION( "of two sides that both skip elements" )
	{
		// Every other element of every other row of a 6 by 8 destination,
		// from every third element of a source of rows of 12.
		std::vector<int> destination(48, -1);
		const auto values = counting(36);
		const auto layout = make_layout({3, 4}, {16, 2}, {12, 3});

		run_dispatched_region_loop(
			copy_kernel(),
			layout,
			destination.data(),
			values.data()
		);

		// Element (i, j) comes from 12i + 3j.
		REQUIRE( destination[0] == 0 );
		REQUIRE( destination[2] == 3 );
		REQUIRE( destination[6] == 9 );
		REQUIRE( destination[16] == 12 );
		REQUIRE( destination[38] == 33 );

		// What lies between is left alone.
		REQUIRE( destination[1] == -1 );
		REQUIRE( destination[8] == -1 );
		REQUIRE( destination[47] == -1 );
	}

	SECTION( "of a side that is walked backwards" )
	{
		std::vector<int> destination(12, -1);
		const auto layout = make_layout({12}, {1}, {-1});

		run_dispatched_region_loop(
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

		run_dispatched_region_loop(
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

		run_dispatched_region_loop(
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

		run_dispatched_region_loop(
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

	run_dispatched_region_loop(
		copy_kernel(),
		layout,
		&destination,
		&source
	);

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

		run_dispatched_region_loop(
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

		run_dispatched_region_loop(
			recording_kernel(visited),
			layout,
			destination.data(),
			source.data()
		);

		REQUIRE( visited.empty() );
	}
}

TEST_CASE(
	"run_region_loop walks a stride of one given at run time as it does the "
	"constant one",
	"[image_region_loop]"
)
{
	const auto source = counting(12);
	const auto layout = make_layout({3, 4}, {4, 1}, {1, 3});
	const std::vector<int> expected({0, 3, 6, 9, 1, 4, 7, 10, 2, 5, 8, 11});

	SECTION( "on the destination" )
	{
		std::vector<int> destination(12, -1);

		run_region_loop(
			copy_kernel(),
			layout,
			std::ptrdiff_t(1),
			std::ptrdiff_t(3),
			destination.data(),
			source.data()
		);

		REQUIRE( destination == expected );
	}

	SECTION( "on both sides" )
	{
		std::vector<int> destination(12, -1);
		const auto contiguous = make_layout({3, 4}, {4, 1}, {4, 1});

		run_region_loop(
			copy_kernel(),
			contiguous,
			std::ptrdiff_t(1),
			std::ptrdiff_t(1),
			destination.data(),
			source.data()
		);

		REQUIRE( destination == source );
	}
}

TEST_CASE(
	"dispatch_region_inner_strides hands a stride of one over as a constant",
	"[image_region_loop]"
)
{
	SECTION( "when both sides are contiguous" )
	{
		const auto strides = dispatch(make_layout({3, 4}, {4, 1}, {4, 1}));

		REQUIRE( strides.calls == 1 );
		REQUIRE( strides.destination_is_unit );
		REQUIRE( strides.source_is_unit );
		REQUIRE( strides.destination_stride == 1 );
		REQUIRE( strides.source_stride == 1 );
	}

	SECTION( "when only the destination is contiguous" )
	{
		const auto strides = dispatch(make_layout({3, 4}, {4, 1}, {1, 3}));

		REQUIRE( strides.calls == 1 );
		REQUIRE( strides.destination_is_unit );
		REQUIRE_FALSE( strides.source_is_unit );
		REQUIRE( strides.destination_stride == 1 );
		REQUIRE( strides.source_stride == 3 );
	}

	SECTION( "when only the source is contiguous" )
	{
		const auto strides =
			dispatch(make_layout({3, 4}, {16, 2}, {4, 1}));

		REQUIRE( strides.calls == 1 );
		REQUIRE_FALSE( strides.destination_is_unit );
		REQUIRE( strides.source_is_unit );
		REQUIRE( strides.destination_stride == 2 );
		REQUIRE( strides.source_stride == 1 );
	}

	SECTION( "and any other stride at run time" )
	{
		const auto strides =
			dispatch(make_layout({3, 4}, {16, 2}, {12, 3}));

		REQUIRE( strides.calls == 1 );
		REQUIRE_FALSE( strides.destination_is_unit );
		REQUIRE_FALSE( strides.source_is_unit );
		REQUIRE( strides.destination_stride == 2 );
		REQUIRE( strides.source_stride == 3 );
	}

	SECTION( "but not a stride of minus one" )
	{
		const auto strides = dispatch(make_layout({12}, {1}, {-1}));

		REQUIRE( strides.calls == 1 );
		REQUIRE( strides.destination_is_unit );
		REQUIRE_FALSE( strides.source_is_unit );
		REQUIRE( strides.source_stride == -1 );
	}

	SECTION( "and a single element as contiguous on both sides" )
	{
		const auto strides = dispatch(make_layout({}, {}, {}));

		REQUIRE( strides.calls == 1 );
		REQUIRE( strides.destination_is_unit );
		REQUIRE( strides.source_is_unit );
	}
}
