// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/const_array.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

using namespace vitrio;

namespace
{

array_descriptor make_descriptor(
	const std::vector<std::size_t> &extents,
	numerical_type data_type = numerical_type::float32
)
{
	return make_contiguous_array_descriptor(make_span(extents), data_type);
}

// An array over a vector of floats, which is also what owns the memory.
array make_array_over(std::shared_ptr<std::vector<float>> storage)
{
	const std::vector<std::size_t> extents = {storage->size()};
	const auto memory = as_bytes(make_span(*storage));

	return array(memory, std::move(storage), make_descriptor(extents));
}

} // anonymous namespace

TEST_CASE(
	"a default constructed array holds nothing",
	"[array]"
)
{
	array arr;
	const array &read_only = arr;

	REQUIRE_FALSE( is_initialized(arr.get_descriptor()) );
	REQUIRE( arr.get_data() == nullptr );
	REQUIRE( read_only.get_data() == nullptr );
}

TEST_CASE(
	"an array refers to the memory it was constructed over",
	"[array]"
)
{
	auto storage = std::make_shared<std::vector<float>>(6);
	const auto memory = as_bytes(make_span(*storage));

	array arr(memory, storage, make_descriptor({2, 3}));
	const array &read_only = arr;

	REQUIRE( arr.get_descriptor() == make_descriptor({2, 3}) );
	REQUIRE( arr.get_data() == memory.data() );
	REQUIRE( read_only.get_data() == memory.data() );
}

TEST_CASE(
	"an array refuses what it cannot be constructed over",
	"[array]"
)
{
	auto storage = std::make_shared<std::vector<float>>(6);
	const auto memory = as_bytes(make_span(*storage));

	SECTION( "a null owner" )
	{
		REQUIRE_THROWS_AS(
			array(memory, nullptr, make_descriptor({2, 3})),
			std::invalid_argument
		);
	}

	SECTION( "a descriptor that describes nothing" )
	{
		REQUIRE_THROWS_AS(
			array(memory, storage, array_descriptor()),
			std::invalid_argument
		);
	}

	SECTION( "memory that is too small" )
	{
		REQUIRE_THROWS_AS(
			array(memory, storage, make_descriptor({7})),
			std::out_of_range
		);
	}

	SECTION( "memory that is not aligned to its elements" )
	{
		const auto shifted = make_span(memory.data() + 1, memory.size() - 1);

		REQUIRE_THROWS_AS(
			array(shifted, storage, make_descriptor({2})),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"an array keeps its memory alive",
	"[array]"
)
{
	auto storage = std::make_shared<std::vector<float>>(6);
	const std::weak_ptr<std::vector<float>> watch = storage;

	auto arr = std::make_unique<array>(make_array_over(std::move(storage)));

	REQUIRE_FALSE( watch.expired() );

	SECTION( "until it is destroyed" )
	{
		arr.reset();

		REQUIRE( watch.expired() );
	}

	SECTION( "until every array sharing it is destroyed" )
	{
		auto first = std::make_unique<array>(arr->share());
		auto second = std::make_unique<const_array>(arr->share_const());

		arr.reset();
		REQUIRE_FALSE( watch.expired() );

		first.reset();
		REQUIRE_FALSE( watch.expired() );

		second.reset();
		REQUIRE( watch.expired() );
	}
}

TEST_CASE(
	"an array cannot be copied, only moved",
	"[array]"
)
{
	REQUIRE_FALSE( std::is_copy_constructible<array>::value );
	REQUIRE_FALSE( std::is_copy_assignable<array>::value );
	REQUIRE( std::is_nothrow_move_constructible<array>::value );
	REQUIRE( std::is_nothrow_move_assignable<array>::value );

	auto source = make_array_over(std::make_shared<std::vector<float>>(6));
	const auto *data = source.get_data();

	SECTION( "move construction leaves the source holding nothing" )
	{
		array destination(std::move(source));

		REQUIRE( destination.get_data() == data );
		REQUIRE( source.get_data() == nullptr );
		REQUIRE_FALSE( is_initialized(source.get_descriptor()) );
	}

	SECTION( "move assignment leaves the source holding nothing" )
	{
		array destination;
		destination = std::move(source);

		REQUIRE( destination.get_data() == data );
		REQUIRE( source.get_data() == nullptr );
	}
}

TEST_CASE(
	"sharing an array gives another one over the same elements",
	"[array]"
)
{
	auto storage = std::make_shared<std::vector<float>>(6);
	auto arr = make_array_over(storage);

	SECTION( "that can be written through" )
	{
		auto alias = arr.share();

		REQUIRE( alias.get_data() == arr.get_data() );
		REQUIRE( alias.get_descriptor() == arr.get_descriptor() );

		// Writing through one is seen through the other.
		*reinterpret_cast<float*>(alias.get_data()) = 3.5f;
		REQUIRE( storage->front() == 3.5f );
	}

	SECTION( "or one that can only be read through" )
	{
		const const_array alias = arr.share_const();

		REQUIRE( alias.get_data() == arr.get_data() );
		REQUIRE( alias.get_descriptor() == arr.get_descriptor() );
	}

	SECTION( "and an array that holds nothing shares nothing" )
	{
		array empty;

		REQUIRE( empty.share().get_data() == nullptr );
		REQUIRE( empty.share_const().get_data() == nullptr );
	}
}

TEST_CASE(
	"an array does not turn into a read-only one unasked",
	"[array]"
)
{
	REQUIRE_FALSE( (std::is_convertible<array, const_array>::value) );
	REQUIRE_FALSE( (std::is_convertible<const_array, array>::value) );
}

TEST_CASE(
	"make_array makes an array with memory of its own",
	"[array]"
)
{
	const auto descriptor = make_descriptor({3, 5}, numerical_type::int32);

	auto arr = make_array(descriptor);

	REQUIRE( arr.get_descriptor() == descriptor );
	REQUIRE( arr.get_data() != nullptr );

	SECTION( "aligned to 64 bytes" )
	{
		const auto address = reinterpret_cast<std::uintptr_t>(arr.get_data());

		REQUIRE( address % 64 == 0 );
	}

	SECTION( "large enough to write every element" )
	{
		auto *elements = reinterpret_cast<std::int32_t*>(arr.get_data());

		std::fill_n(elements, 15, 7);

		REQUIRE( std::count(elements, elements + 15, 7) == 15 );
	}

	SECTION( "apart from that of any other array" )
	{
		auto other = make_array(descriptor);

		REQUIRE( other.get_data() != arr.get_data() );
	}
}

TEST_CASE(
	"make_array makes arrays of every shape",
	"[array]"
)
{
	SECTION( "a single element" )
	{
		auto arr = make_array(make_descriptor({}));

		REQUIRE( arr.get_data() != nullptr );
	}

	SECTION( "no elements, which is not the same as holding nothing" )
	{
		auto arr = make_array(make_descriptor({4, 0}));

		REQUIRE( is_initialized(arr.get_descriptor()) );
		REQUIRE( arr.get_data() != nullptr );
	}

	SECTION( "elements that do not start at the first byte" )
	{
		const array_descriptor descriptor(
			{4},
			{-1},
			3,
			numerical_type::float64
		);

		auto arr = make_array(descriptor);
		auto *elements = reinterpret_cast<double*>(arr.get_data());

		// The origin is at offset three, and the other three before it.
		std::fill_n(elements, 4, 1.0);

		REQUIRE( arr.get_descriptor() == descriptor );
	}
}

TEST_CASE(
	"make_array refuses a descriptor it cannot allocate for",
	"[array]"
)
{
	SECTION( "one that describes nothing" )
	{
		REQUIRE_THROWS_AS(
			make_array(array_descriptor()),
			std::invalid_argument
		);
	}

	SECTION( "one with an element before the first byte" )
	{
		const array_descriptor descriptor(
			{4},
			{-1},
			0,
			numerical_type::float64
		);

		REQUIRE_THROWS_AS( make_array(descriptor), std::out_of_range );
	}
}
