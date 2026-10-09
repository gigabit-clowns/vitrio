// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/const_array.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
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

// A read-only array over a vector of floats, which is also what owns the
// memory.
const_array make_array_over(std::shared_ptr<const std::vector<float>> storage)
{
	const std::vector<std::size_t> extents = {storage->size()};
	const auto memory = as_bytes(make_span(*storage));

	return const_array(memory, std::move(storage), make_descriptor(extents));
}

} // anonymous namespace

TEST_CASE(
	"a default constructed const_array holds nothing",
	"[const_array]"
)
{
	const const_array arr;

	REQUIRE_FALSE( is_initialized(arr.get_descriptor()) );
	REQUIRE( arr.get_data() == nullptr );
}

TEST_CASE(
	"a const_array refers to the read-only memory it was constructed over",
	"[const_array]"
)
{
	const auto storage = std::make_shared<const std::vector<float>>(6, 1.5f);
	const auto memory = as_bytes(make_span(*storage));

	const const_array arr(memory, storage, make_descriptor({2, 3}));

	REQUIRE( arr.get_descriptor() == make_descriptor({2, 3}) );
	REQUIRE( arr.get_data() == memory.data() );
}

TEST_CASE(
	"a const_array refuses what it cannot be constructed over",
	"[const_array]"
)
{
	const auto storage = std::make_shared<const std::vector<float>>(6);
	const auto memory = as_bytes(make_span(*storage));

	SECTION( "a null owner" )
	{
		REQUIRE_THROWS_AS(
			const_array(memory, nullptr, make_descriptor({2, 3})),
			std::invalid_argument
		);
	}

	SECTION( "a descriptor that describes nothing" )
	{
		REQUIRE_THROWS_AS(
			const_array(memory, storage, array_descriptor()),
			std::invalid_argument
		);
	}

	SECTION( "memory that is too small" )
	{
		REQUIRE_THROWS_AS(
			const_array(memory, storage, make_descriptor({7})),
			std::out_of_range
		);
	}

	SECTION( "memory that is not aligned to its elements" )
	{
		const auto shifted = make_span(memory.data() + 1, memory.size() - 1);

		REQUIRE_THROWS_AS(
			const_array(shifted, storage, make_descriptor({2})),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"a const_array keeps its memory alive",
	"[const_array]"
)
{
	auto storage = std::make_shared<const std::vector<float>>(6);
	const std::weak_ptr<const std::vector<float>> watch = storage;

	auto arr =
		std::make_unique<const_array>(make_array_over(std::move(storage)));

	REQUIRE_FALSE( watch.expired() );

	SECTION( "until it is destroyed" )
	{
		arr.reset();

		REQUIRE( watch.expired() );
	}

	SECTION( "until every array sharing it is destroyed" )
	{
		auto alias = std::make_unique<const_array>(arr->share());

		arr.reset();
		REQUIRE_FALSE( watch.expired() );

		alias.reset();
		REQUIRE( watch.expired() );
	}
}

TEST_CASE(
	"a const_array outlives the array it was shared from",
	"[const_array]"
)
{
	auto storage = std::make_shared<std::vector<float>>(6, 2.5f);
	const std::weak_ptr<std::vector<float>> watch = storage;
	const auto memory = as_bytes(make_span(*storage));
	auto source = std::make_unique<array>(
		memory,
		std::move(storage),
		make_descriptor({6})
	);

	const const_array arr = source->share_const();
	source.reset();

	REQUIRE_FALSE( watch.expired() );
	REQUIRE( arr.get_data() == memory.data() );
	REQUIRE( *reinterpret_cast<const float*>(arr.get_data()) == 2.5f );
}

TEST_CASE(
	"a const_array cannot be copied, only moved",
	"[const_array]"
)
{
	REQUIRE_FALSE( std::is_copy_constructible<const_array>::value );
	REQUIRE_FALSE( std::is_copy_assignable<const_array>::value );
	REQUIRE( std::is_nothrow_move_constructible<const_array>::value );
	REQUIRE( std::is_nothrow_move_assignable<const_array>::value );

	auto source =
		make_array_over(std::make_shared<const std::vector<float>>(6));
	const auto *data = source.get_data();

	SECTION( "move construction leaves the source holding nothing" )
	{
		const const_array destination(std::move(source));

		REQUIRE( destination.get_data() == data );
		REQUIRE( source.get_data() == nullptr );
		REQUIRE_FALSE( is_initialized(source.get_descriptor()) );
	}

	SECTION( "move assignment leaves the source holding nothing" )
	{
		const_array destination;
		destination = std::move(source);

		REQUIRE( destination.get_data() == data );
		REQUIRE( source.get_data() == nullptr );
	}
}

TEST_CASE(
	"a const_array gives no way to write its elements",
	"[const_array]"
)
{
	const auto returns_read_only = std::is_same<
		decltype(std::declval<const_array&>().get_data()),
		const byte*
	>::value;

	REQUIRE( returns_read_only );
}
