// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <array/array_memory.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace vitrio;

namespace
{

array_descriptor make_descriptor(
	const std::vector<std::size_t> &extents,
	const std::vector<std::ptrdiff_t> &strides,
	std::ptrdiff_t offset = 0,
	numerical_type data_type = numerical_type::float32
)
{
	return array_descriptor(extents, strides, offset, data_type);
}

} // anonymous namespace

TEST_CASE(
	"compute_storage_requirement counts the bytes through the last element",
	"[array_memory]"
)
{
	SECTION( "of contiguous elements" )
	{
		const auto descriptor = make_descriptor({2, 3, 4}, {12, 4, 1});

		REQUIRE( compute_storage_requirement(descriptor) == 24 * 4 );
	}

	SECTION( "of a single element" )
	{
		const auto descriptor = make_descriptor({}, {});

		REQUIRE( compute_storage_requirement(descriptor) == 4 );
	}

	SECTION( "of elements of another size" )
	{
		const auto descriptor = make_descriptor(
			{2, 3},
			{3, 1},
			0,
			numerical_type::complex_float64
		);

		REQUIRE( compute_storage_requirement(descriptor) == 6 * 16 );
	}

	SECTION( "including what lies before the origin" )
	{
		const auto descriptor = make_descriptor({2, 3}, {3, 1}, 10);

		REQUIRE( compute_storage_requirement(descriptor) == 16 * 4 );
	}

	SECTION( "including the elements a stride skips" )
	{
		// Offsets 0, 2 and 4, so five elements are spanned.
		const auto descriptor = make_descriptor({3}, {2});

		REQUIRE( compute_storage_requirement(descriptor) == 5 * 4 );
	}

	SECTION( "of elements walked backwards" )
	{
		// Offsets 2, 1 and 0.
		const auto descriptor = make_descriptor({3}, {-1}, 2);

		REQUIRE( compute_storage_requirement(descriptor) == 3 * 4 );
	}

	SECTION( "of elements that share one position" )
	{
		const auto descriptor = make_descriptor({5}, {0}, 3);

		REQUIRE( compute_storage_requirement(descriptor) == 4 * 4 );
	}
}

TEST_CASE(
	"compute_storage_requirement is zero when there is no element",
	"[array_memory]"
)
{
	SECTION( "whichever extent is zero" )
	{
		REQUIRE(
			compute_storage_requirement(make_descriptor({0, 3}, {3, 1})) == 0
		);
		REQUIRE(
			compute_storage_requirement(make_descriptor({2, 0}, {0, 1})) == 0
		);
	}

	SECTION( "whatever the offset and the strides say" )
	{
		const auto descriptor = make_descriptor({0, 3}, {-100, 7}, -5);

		REQUIRE( compute_storage_requirement(descriptor) == 0 );
	}
}

TEST_CASE(
	"compute_storage_requirement refuses elements that cannot be reached",
	"[array_memory]"
)
{
	SECTION( "an origin before the memory" )
	{
		REQUIRE_THROWS_AS(
			compute_storage_requirement(make_descriptor({2}, {1}, -1)),
			std::out_of_range
		);
	}

	SECTION( "a negative stride walking before the memory" )
	{
		// Offsets 1, 0 and -1.
		REQUIRE_THROWS_AS(
			compute_storage_requirement(make_descriptor({3}, {-1}, 1)),
			std::out_of_range
		);
	}

	SECTION( "a reach that overflows" )
	{
		const auto highest = std::numeric_limits<std::ptrdiff_t>::max();

		REQUIRE_THROWS_AS(
			compute_storage_requirement(make_descriptor({3}, {highest})),
			std::out_of_range
		);
		REQUIRE_THROWS_AS(
			compute_storage_requirement(
				make_descriptor({2, 2}, {highest, highest})
			),
			std::out_of_range
		);
	}

	SECTION( "an extent no offset can count" )
	{
		const auto huge = std::numeric_limits<std::size_t>::max();

		REQUIRE_THROWS_AS(
			compute_storage_requirement(make_descriptor({huge}, {1})),
			std::out_of_range
		);
	}

	SECTION( "a size in bytes that overflows" )
	{
		const auto highest = std::numeric_limits<std::ptrdiff_t>::max();
		const auto descriptor = make_descriptor(
			{2},
			{highest - 1},
			0,
			numerical_type::float64
		);

		REQUIRE_THROWS_AS(
			compute_storage_requirement(descriptor),
			std::out_of_range
		);
	}
}

TEST_CASE(
	"check_array_memory accepts memory that holds every element",
	"[array_memory]"
)
{
	std::vector<float> storage(24);
	const auto memory = as_bytes(make_span(storage));

	SECTION( "exactly" )
	{
		REQUIRE_NOTHROW(
			check_array_memory(memory, make_descriptor({2, 3, 4}, {12, 4, 1}))
		);
	}

	SECTION( "with room to spare" )
	{
		REQUIRE_NOTHROW(
			check_array_memory(memory, make_descriptor({2, 3}, {3, 1}, 4))
		);
	}

	SECTION( "and no memory when there is no element" )
	{
		REQUIRE_NOTHROW(
			check_array_memory(
				span<const byte>(),
				make_descriptor({0, 3}, {3, 1})
			)
		);
	}
}

TEST_CASE(
	"check_array_memory refuses memory that cannot be read as stated",
	"[array_memory]"
)
{
	std::vector<float> storage(24);
	const auto memory = as_bytes(make_span(storage));

	SECTION( "a descriptor that describes nothing" )
	{
		REQUIRE_THROWS_AS(
			check_array_memory(memory, array_descriptor()),
			std::invalid_argument
		);
	}

	SECTION( "one element too many" )
	{
		REQUIRE_THROWS_AS(
			check_array_memory(memory, make_descriptor({5, 5}, {5, 1})),
			std::out_of_range
		);
	}

	SECTION( "an offset that pushes the last element out" )
	{
		REQUIRE_THROWS_AS(
			check_array_memory(
				memory,
				make_descriptor({2, 3, 4}, {12, 4, 1}, 1)
			),
			std::out_of_range
		);
	}

	SECTION( "an element before the memory" )
	{
		REQUIRE_THROWS_AS(
			check_array_memory(memory, make_descriptor({3}, {-1}, 1)),
			std::out_of_range
		);
	}

	SECTION( "memory that is not aligned to its elements" )
	{
		const auto shifted = make_span(memory.data() + 1, memory.size() - 1);

		REQUIRE_THROWS_AS(
			check_array_memory(shifted, make_descriptor({2}, {1})),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"check_array_memory aligns a complex number as its components",
	"[array_memory]"
)
{
	std::vector<double> storage(8);
	const auto memory = as_bytes(make_span(storage));

	// Four bytes in, which suits a float and not a double.
	const auto shifted = make_span(memory.data() + 4, memory.size() - 4);

	REQUIRE_NOTHROW(
		check_array_memory(
			shifted,
			make_descriptor({2}, {1}, 0, numerical_type::complex_float32)
		)
	);
	REQUIRE_THROWS_AS(
		check_array_memory(
			shifted,
			make_descriptor({2}, {1}, 0, numerical_type::complex_float64)
		),
		std::invalid_argument
	);
}
