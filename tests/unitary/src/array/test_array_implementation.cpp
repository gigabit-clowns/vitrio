// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <array/array_implementation.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace vitrio;

namespace
{

array_descriptor make_descriptor(const std::vector<std::size_t> &extents)
{
	return make_contiguous_array_descriptor(
		make_span(extents),
		numerical_type::float32
	);
}

} // anonymous namespace

TEST_CASE(
	"an array_implementation holds the memory and the descriptor it is given",
	"[array_implementation]"
)
{
	std::vector<float> storage(6);
	const auto memory = as_bytes(make_span(storage));

	const array_implementation implementation(
		memory,
		nullptr,
		make_descriptor({2, 3})
	);

	REQUIRE( implementation.get_data() == memory.data() );
	REQUIRE( implementation.get_descriptor() == make_descriptor({2, 3}) );
}

TEST_CASE(
	"an array_implementation keeps its owner until it is destroyed",
	"[array_implementation]"
)
{
	auto storage = std::make_shared<std::vector<float>>(6);
	const std::weak_ptr<std::vector<float>> watch = storage;
	const auto memory = as_bytes(make_span(*storage));

	auto implementation = std::make_unique<array_implementation>(
		memory,
		std::move(storage),
		make_descriptor({2, 3})
	);

	REQUIRE_FALSE( watch.expired() );

	implementation.reset();

	REQUIRE( watch.expired() );
}

TEST_CASE(
	"an array_implementation refuses memory that cannot be read as stated",
	"[array_implementation]"
)
{
	std::vector<float> storage(6);
	const auto memory = as_bytes(make_span(storage));

	SECTION( "a descriptor that describes nothing" )
	{
		REQUIRE_THROWS_AS(
			array_implementation(memory, nullptr, array_descriptor()),
			std::invalid_argument
		);
	}

	SECTION( "memory that is too small" )
	{
		REQUIRE_THROWS_AS(
			array_implementation(memory, nullptr, make_descriptor({7})),
			std::out_of_range
		);
	}

	SECTION( "memory that is not aligned" )
	{
		const auto shifted = make_span(memory.data() + 1, memory.size() - 1);

		REQUIRE_THROWS_AS(
			array_implementation(shifted, nullptr, make_descriptor({2})),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"the empty descriptor of array_implementation describes nothing",
	"[array_implementation]"
)
{
	const auto &descriptor = array_implementation::get_empty_descriptor();

	REQUIRE_FALSE( is_initialized(descriptor) );

	// It is one object, so a reference to it stays valid.
	REQUIRE( &descriptor == &array_implementation::get_empty_descriptor() );
}
