// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <array/array_data.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/const_array.hpp>
#include <vitrio/array/const_array_ref.hpp>

#include <cstddef>
#include <stdexcept>
#include <vector>

using namespace vitrio;

namespace
{

array make_small_array()
{
	const std::vector<std::size_t> extents = {2, 3};

	return make_array(
		make_contiguous_array_descriptor(
			make_span(extents),
			numerical_type::float32
		)
	);
}

} // anonymous namespace

TEST_CASE(
	"get_array_data gives the memory of an array",
	"[array_data]"
)
{
	auto arr = make_small_array();

	SECTION( "to write through" )
	{
		REQUIRE( get_array_data(array_ref(arr)) == arr.get_data() );
	}

	SECTION( "to read through" )
	{
		const const_array read_only = arr.share_const();

		REQUIRE(
			get_array_data(const_array_ref(read_only)) == read_only.get_data()
		);
	}
}

TEST_CASE(
	"get_array_data refuses an array that is not initialized",
	"[array_data]"
)
{
	SECTION( "to write through" )
	{
		array empty;

		REQUIRE_THROWS_AS(
			get_array_data(array_ref(empty)),
			std::invalid_argument
		);
		REQUIRE_THROWS_AS( get_array_data(array_ref()), std::invalid_argument );
	}

	SECTION( "to read through" )
	{
		const const_array empty;

		REQUIRE_THROWS_AS(
			get_array_data(const_array_ref(empty)),
			std::invalid_argument
		);
		REQUIRE_THROWS_AS(
			get_array_data(const_array_ref()),
			std::invalid_argument
		);
	}
}
