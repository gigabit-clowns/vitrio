// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <array/borrowed_array.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/const_array.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <vitrio/tests/host_array.hpp>

#include <memory>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

TEST_CASE(
	"borrow_array makes an array over the memory of a reference",
	"[borrowed_array]"
)
{
	auto original =
		make_host_array<float>({2, 3}, numerical_type::float32, 1.5f);

	SECTION( "writable" )
	{
		auto borrowed = borrow_array(array_ref(original));

		CHECK( borrowed.get_data() == original.get_data() );
		CHECK( borrowed.get_descriptor() == original.get_descriptor() );
	}

	SECTION( "read only" )
	{
		const auto borrowed = borrow_array(const_array_ref(original));

		CHECK( borrowed.get_data() == original.get_data() );
		CHECK( borrowed.get_descriptor() == original.get_descriptor() );
	}
}

TEST_CASE(
	"borrow_array of a reference to nothing holds nothing",
	"[borrowed_array]"
)
{
	CHECK( borrow_array(array_ref()).get_data() == nullptr );
	CHECK( borrow_array(const_array_ref()).get_data() == nullptr );
}

TEST_CASE(
	"borrow_array keeps nothing alive",
	"[borrowed_array]"
)
{
	auto storage = std::make_shared<std::vector<float>>(4, 0.0f);
	const std::weak_ptr<std::vector<float>> watched = storage;
	const auto descriptor =
		make_host_array<float>({4}, numerical_type::float32).get_descriptor();

	array borrowed;
	{
		array original(
			span<byte>(
				reinterpret_cast<byte*>(storage->data()),
				storage->size() * sizeof(float)
			),
			storage,
			descriptor
		);
		borrowed = borrow_array(array_ref(original));
	}
	storage.reset();

	// The memory is released although an array borrowed over it lives on.
	CHECK( watched.expired() );
	CHECK( borrowed.get_data() != nullptr );
}
