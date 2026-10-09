// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_ref.hpp>
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

} // anonymous namespace

TEST_CASE(
	"a default constructed array_ref refers to nothing",
	"[array_ref]"
)
{
	const array_ref ref;

	REQUIRE_FALSE( is_initialized(ref.get_descriptor()) );
	REQUIRE( ref.get_data() == nullptr );
}

TEST_CASE(
	"an array_ref refers to memory and a descriptor the caller keeps alive",
	"[array_ref]"
)
{
	std::vector<float> storage(6);
	const auto memory = as_bytes(make_span(storage));
	const auto descriptor = make_descriptor({2, 3});

	const array_ref ref(memory, descriptor);

	REQUIRE( ref.get_data() == memory.data() );

	SECTION( "to that very descriptor, not to a copy of it" )
	{
		REQUIRE( &ref.get_descriptor() == &descriptor );
	}

	SECTION( "and writes to that memory" )
	{
		*reinterpret_cast<float*>(ref.get_data()) = 4.5f;

		REQUIRE( storage.front() == 4.5f );
	}
}

TEST_CASE(
	"an array_ref refuses memory that cannot be read as stated",
	"[array_ref]"
)
{
	std::vector<float> storage(6);
	const auto memory = as_bytes(make_span(storage));

	SECTION( "a descriptor that describes nothing" )
	{
		const array_descriptor descriptor;

		REQUIRE_THROWS_AS(
			array_ref(memory, descriptor),
			std::invalid_argument
		);
	}

	SECTION( "memory that is too small" )
	{
		const auto descriptor = make_descriptor({7});

		REQUIRE_THROWS_AS(
			array_ref(memory, descriptor),
			std::out_of_range
		);
	}

	SECTION( "memory that is not aligned to its elements" )
	{
		const auto shifted = make_span(memory.data() + 1, memory.size() - 1);
		const auto descriptor = make_descriptor({2});

		REQUIRE_THROWS_AS(
			array_ref(shifted, descriptor),
			std::invalid_argument
		);
	}
}

TEST_CASE(
	"an array_ref cannot be made over a temporary descriptor",
	"[array_ref]"
)
{
	const auto from_named = std::is_constructible<
		array_ref,
		span<byte>,
		const array_descriptor&
	>::value;
	const auto from_temporary = std::is_constructible<
		array_ref,
		span<byte>,
		array_descriptor
	>::value;

	REQUIRE( from_named );
	REQUIRE_FALSE( from_temporary );
}

TEST_CASE(
	"an array converts to a reference to itself",
	"[array_ref]"
)
{
	auto arr = make_array(make_descriptor({2, 3}));

	const array_ref ref = arr;

	REQUIRE( ref.get_data() == arr.get_data() );
	REQUIRE( &ref.get_descriptor() == &arr.get_descriptor() );

	SECTION( "and one that holds nothing to a reference to nothing" )
	{
		array empty;

		const array_ref empty_ref = empty;

		REQUIRE( empty_ref.get_data() == nullptr );
		REQUIRE_FALSE( is_initialized(empty_ref.get_descriptor()) );
	}
}

TEST_CASE(
	"only an array that can be written through converts to an array_ref",
	"[array_ref]"
)
{
	REQUIRE( (std::is_convertible<array&, array_ref>::value) );

	REQUIRE_FALSE( (std::is_convertible<const array&, array_ref>::value) );
	REQUIRE_FALSE( (std::is_convertible<const_array&, array_ref>::value) );

	// A temporary array would be gone before the reference is used.
	REQUIRE_FALSE( (std::is_convertible<array, array_ref>::value) );
}

TEST_CASE(
	"an array_ref is two pointers and no more",
	"[array_ref]"
)
{
	REQUIRE( std::is_trivially_copyable<array_ref>::value );
	REQUIRE( sizeof(array_ref) == 2 * sizeof(void*) );

	std::vector<float> storage(6);
	const auto descriptor = make_descriptor({6});
	const array_ref ref(as_bytes(make_span(storage)), descriptor);

	SECTION( "so a copy refers to the same array" )
	{
		const array_ref copy(ref);

		REQUIRE( copy.get_data() == ref.get_data() );
		REQUIRE( &copy.get_descriptor() == &ref.get_descriptor() );
	}

	SECTION( "and so does one that is assigned" )
	{
		array_ref copy;
		copy = ref;

		REQUIRE( copy.get_data() == ref.get_data() );
		REQUIRE( &copy.get_descriptor() == &ref.get_descriptor() );
	}
}

TEST_CASE(
	"an array_ref does not keep the array it refers to alive",
	"[array_ref]"
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

	const array_ref ref = *arr;
	arr.reset();

	// The memory is released although the reference is still there. What it
	// points to is gone, so it is not looked at.
	REQUIRE( watch.expired() );
	static_cast<void>(ref);
}
