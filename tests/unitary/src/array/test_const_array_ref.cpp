// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/const_array.hpp>
#include <vitrio/array/const_array_ref.hpp>

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

} // anonymous namespace

TEST_CASE(
	"a default constructed const_array_ref refers to nothing",
	"[const_array_ref]"
)
{
	const const_array_ref ref;

	REQUIRE_FALSE( is_initialized(ref.get_descriptor()) );
	REQUIRE( ref.get_data() == nullptr );
}

TEST_CASE(
	"a const_array_ref refers to read-only memory the caller keeps alive",
	"[const_array_ref]"
)
{
	const std::vector<float> storage(6, 1.5f);
	const auto memory = as_bytes(make_span(storage));
	const auto descriptor = make_descriptor({2, 3});

	const const_array_ref ref(memory, descriptor);

	REQUIRE( ref.get_data() == memory.data() );
	REQUIRE( &ref.get_descriptor() == &descriptor );
	REQUIRE( *reinterpret_cast<const float*>(ref.get_data()) == 1.5f );
}

TEST_CASE(
	"a const_array_ref refuses memory that cannot be read as stated",
	"[const_array_ref]"
)
{
	const std::vector<float> storage(6);
	const auto memory = as_bytes(make_span(storage));

	SECTION( "a descriptor that describes nothing" )
	{
		const array_descriptor descriptor;

		REQUIRE_THROWS_AS(
			const_array_ref(memory, descriptor),
			std::invalid_argument
		);
	}

	SECTION( "memory that is too small" )
	{
		const auto descriptor = make_descriptor({7});

		REQUIRE_THROWS_AS(
			const_array_ref(memory, descriptor),
			std::out_of_range
		);
	}

	SECTION( "memory that is not aligned to its elements" )
	{
		const auto shifted = make_span(memory.data() + 1, memory.size() - 1);
		const auto descriptor = make_descriptor({2});

		REQUIRE_THROWS_AS(
			const_array_ref(shifted, descriptor),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"a const_array_ref cannot be made over a temporary descriptor",
	"[const_array_ref]"
)
{
	const auto from_named = std::is_constructible<
		const_array_ref,
		span<const byte>,
		const array_descriptor&
	>::value;
	const auto from_temporary = std::is_constructible<
		const_array_ref,
		span<const byte>,
		array_descriptor
	>::value;

	REQUIRE( from_named );
	REQUIRE_FALSE( from_temporary );
}

TEST_CASE(
	"every array class converts to a const_array_ref to its elements",
	"[const_array_ref]"
)
{
	auto arr = make_array(make_descriptor({2, 3}));

	SECTION( "an array" )
	{
		const const_array_ref ref = arr;

		REQUIRE( ref.get_data() == arr.get_data() );
		REQUIRE( &ref.get_descriptor() == &arr.get_descriptor() );
	}

	SECTION( "an array that cannot be written through" )
	{
		const array &read_only = arr;

		const const_array_ref ref = read_only;

		REQUIRE( ref.get_data() == arr.get_data() );
	}

	SECTION( "a const_array" )
	{
		const const_array shared = arr.share_const();

		const const_array_ref ref = shared;

		REQUIRE( ref.get_data() == arr.get_data() );
		REQUIRE( ref.get_descriptor() == arr.get_descriptor() );
	}

	SECTION( "an array_ref" )
	{
		const array_ref writable = arr;

		const const_array_ref ref = writable;

		REQUIRE( ref.get_data() == arr.get_data() );
		REQUIRE( &ref.get_descriptor() == &arr.get_descriptor() );
	}

	SECTION( "and one that holds nothing to a reference to nothing" )
	{
		const const_array empty;

		const const_array_ref ref = empty;

		REQUIRE( ref.get_data() == nullptr );
		REQUIRE_FALSE( is_initialized(ref.get_descriptor()) );
	}
}

TEST_CASE(
	"a const_array_ref does not convert back to what can be written through",
	"[const_array_ref]"
)
{
	REQUIRE_FALSE( (std::is_convertible<const_array_ref, array_ref>::value) );

	const auto returns_read_only = std::is_same<
		decltype(std::declval<const_array_ref&>().get_data()),
		const byte*
	>::value;

	REQUIRE( returns_read_only );
}

TEST_CASE(
	"a const_array_ref is two pointers and no more",
	"[const_array_ref]"
)
{
	REQUIRE( std::is_trivially_copyable<const_array_ref>::value );
	REQUIRE( sizeof(const_array_ref) == 2 * sizeof(void*) );

	const std::vector<float> storage(6);
	const auto descriptor = make_descriptor({6});
	const const_array_ref ref(as_bytes(make_span(storage)), descriptor);

	SECTION( "so a copy refers to the same array" )
	{
		const const_array_ref copy(ref);

		REQUIRE( copy.get_data() == ref.get_data() );
		REQUIRE( &copy.get_descriptor() == &ref.get_descriptor() );
	}

	SECTION( "and so does one that is assigned" )
	{
		const_array_ref copy;
		copy = ref;

		REQUIRE( copy.get_data() == ref.get_data() );
		REQUIRE( &copy.get_descriptor() == &ref.get_descriptor() );
	}
}

TEST_CASE(
	"a const_array_ref does not keep the array it refers to alive",
	"[const_array_ref]"
)
{
	auto storage = std::make_shared<std::vector<float>>(6);
	const std::weak_ptr<std::vector<float>> watch = storage;
	const auto memory = as_bytes(make_span(*storage));
	auto arr = std::make_unique<array>(
		memory,
		std::move(storage),
		make_descriptor({6})
	);

	const const_array_ref ref = *arr;
	arr.reset();

	// The memory is released although the reference is still there. What it
	// points to is gone, so it is not looked at.
	REQUIRE( watch.expired() );
	static_cast<void>(ref);
}
