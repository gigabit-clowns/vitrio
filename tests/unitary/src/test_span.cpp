// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/span.hpp>

#include <array>
#include <type_traits>
#include <vector>

using namespace vitrio;

TEST_CASE(
	"a default constructed span has no elements",
	"[span]"
)
{
	const span<int> values;

	REQUIRE( values.size() == 0 );
	REQUIRE( values.empty() );
	REQUIRE( values.data() == nullptr );
	REQUIRE( values.begin() == values.end() );
}

TEST_CASE(
	"a span refers to the elements it was constructed over",
	"[span]"
)
{
	std::array<int, 4> storage = {10, 20, 30, 40};

	const span<int> values(storage.data(), storage.size());

	SECTION( "it reports where they are and how many" )
	{
		REQUIRE( values.data() == storage.data() );
		REQUIRE( values.size() == 4 );
		REQUIRE_FALSE( values.empty() );
	}

	SECTION( "it gives each of them by position" )
	{
		REQUIRE( values[0] == 10 );
		REQUIRE( values[3] == 40 );
		REQUIRE( values.front() == 10 );
		REQUIRE( values.back() == 40 );
	}

	SECTION( "it iterates over them in order" )
	{
		const std::vector<int> visited(values.begin(), values.end());

		REQUIRE( visited == std::vector<int>({10, 20, 30, 40}) );
	}

	SECTION( "writing through it changes them" )
	{
		values[1] = 21;
		values.back() = 41;

		REQUIRE( storage[1] == 21 );
		REQUIRE( storage[3] == 41 );
	}
}

TEST_CASE(
	"copying a span copies the reference and not the elements",
	"[span]"
)
{
	std::array<int, 3> storage = {1, 2, 3};
	const span<int> values(storage.data(), storage.size());

	const span<int> copy = values;

	REQUIRE( copy.data() == storage.data() );
	REQUIRE( copy.size() == 3 );

	// The copy writes to the same elements.
	copy[0] = 7;
	REQUIRE( values[0] == 7 );
}

TEST_CASE(
	"a span of non-const elements converts to one of const elements",
	"[span]"
)
{
	std::array<int, 3> storage = {1, 2, 3};
	const span<int> values(storage.data(), storage.size());

	const span<const int> read_only = values;

	REQUIRE( read_only.data() == storage.data() );
	REQUIRE( read_only.size() == 3 );

	SECTION( "but not the other way around" )
	{
		const auto converts_back =
			std::is_convertible<span<const int>, span<int>>::value;

		REQUIRE_FALSE( converts_back );
	}

	SECTION( "nor to a span of another type" )
	{
		const auto converts_to_long =
			std::is_convertible<span<int>, span<const long>>::value;

		REQUIRE_FALSE( converts_to_long );
	}
}

TEST_CASE(
	"a span can be used in a constant expression",
	"[span]"
)
{
	static constexpr std::array<int, 3> storage = {{4, 5, 6}};

	constexpr span<const int> values(&storage[0], 3);

	static_assert(values.size() == 3, "The size is constant");
	static_assert(!values.empty(), "Emptiness is constant");
	static_assert(values[1] == 5, "An element is constant");
	static_assert(values.front() == 4, "The first element is constant");
	static_assert(values.back() == 6, "The last element is constant");
	static_assert(values.end() - values.begin() == 3, "Iterators are constant");
}

TEST_CASE(
	"make_span from a pointer keeps the constness of what it points to",
	"[span]"
)
{
	std::array<int, 2> storage = {1, 2};
	const int *read_only_data = storage.data();

	const auto values = make_span(storage.data(), storage.size());
	const auto read_only = make_span(read_only_data, storage.size());

	REQUIRE( (std::is_same<decltype(values), const span<int>>::value) );
	REQUIRE(
		(std::is_same<decltype(read_only), const span<const int>>::value)
	);
	REQUIRE( values.data() == storage.data() );
	REQUIRE( values.size() == 2 );
	REQUIRE( read_only.data() == storage.data() );
	REQUIRE( read_only.size() == 2 );
}

TEST_CASE(
	"make_span covers the elements of a vector",
	"[span]"
)
{
	SECTION( "a vector gives a span of its element type" )
	{
		std::vector<int> storage = {1, 2, 3};

		const auto values = make_span(storage);

		REQUIRE( (std::is_same<decltype(values), const span<int>>::value) );
		REQUIRE( values.data() == storage.data() );
		REQUIRE( values.size() == 3 );
	}

	SECTION( "a const vector gives a read-only span" )
	{
		const std::vector<int> storage = {1, 2, 3};

		const auto values = make_span(storage);

		REQUIRE(
			(std::is_same<decltype(values), const span<const int>>::value)
		);
		REQUIRE( values.data() == storage.data() );
		REQUIRE( values.size() == 3 );
	}

	SECTION( "an empty vector gives an empty span" )
	{
		const std::vector<int> storage;

		REQUIRE( make_span(storage).empty() );
	}
}

TEST_CASE(
	"make_span covers the elements of an array",
	"[span]"
)
{
	SECTION( "an array gives a span of its element type" )
	{
		std::array<int, 3> storage = {1, 2, 3};

		const auto values = make_span(storage);

		REQUIRE( (std::is_same<decltype(values), const span<int>>::value) );
		REQUIRE( values.data() == storage.data() );
		REQUIRE( values.size() == 3 );
	}

	SECTION( "a const array gives a read-only span" )
	{
		const std::array<int, 3> storage = {1, 2, 3};

		const auto values = make_span(storage);

		REQUIRE(
			(std::is_same<decltype(values), const span<const int>>::value)
		);
		REQUIRE( values.data() == storage.data() );
		REQUIRE( values.size() == 3 );
	}
}

TEST_CASE(
	"as_bytes covers the bytes the elements are stored in",
	"[span]"
)
{
	SECTION( "of writable elements, as writable bytes" )
	{
		std::array<int, 3> storage = {1, 2, 3};

		const auto bytes = as_bytes(make_span(storage));

		REQUIRE( (std::is_same<decltype(bytes), const span<byte>>::value) );
		REQUIRE( static_cast<void*>(bytes.data()) == storage.data() );
		REQUIRE( bytes.size() == 3 * sizeof(int) );
	}

	SECTION( "of read-only elements, as read-only bytes" )
	{
		const std::array<int, 3> storage = {1, 2, 3};

		const auto bytes = as_bytes(make_span(storage));

		REQUIRE(
			(std::is_same<decltype(bytes), const span<const byte>>::value)
		);
		REQUIRE( static_cast<const void*>(bytes.data()) == storage.data() );
		REQUIRE( bytes.size() == 3 * sizeof(int) );
	}

	SECTION( "and writing a byte changes the element it belongs to" )
	{
		std::array<unsigned char, 2> storage = {1, 2};

		as_bytes(make_span(storage))[1] = 9;

		REQUIRE( storage[1] == 9 );
	}
}
