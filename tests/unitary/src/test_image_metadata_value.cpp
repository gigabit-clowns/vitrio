// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_metadata_value.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

using namespace vitrio;

// What makes no value does not compile, which a test can only tell from
// whether the value is constructible.
static_assert(
	!std::is_constructible<image_metadata_value, bool>::value,
	"A boolean makes no value"
);
static_assert(
	!std::is_constructible<image_metadata_value, std::nullptr_t>::value,
	"A null pointer makes no value"
);
static_assert(
	!std::is_constructible<image_metadata_value, const void*>::value,
	"A pointer that is not a string makes no value"
);
static_assert(
	!std::is_constructible<image_metadata_value, int*>::value,
	"A pointer to a number makes no value"
);
static_assert(
	std::is_convertible<int, image_metadata_value>::value,
	"An integer is given where a value is taken"
);
static_assert(
	std::is_convertible<double, image_metadata_value>::value,
	"A real is given where a value is taken"
);
static_assert(
	std::is_convertible<const char*, image_metadata_value>::value,
	"A string literal is given where a value is taken"
);
static_assert(
	std::is_nothrow_move_constructible<image_metadata_value>::value,
	"A value moves without throwing"
);

TEST_CASE(
	"image_metadata_value is an integer when made from an integral type",
	"[image_metadata_value]"
)
{
	SECTION( "an int" )
	{
		const image_metadata_value value(42);

		REQUIRE( value.get_type() == image_metadata_value_type::integer );
		REQUIRE( value.get_integer() == 42 );
	}

	SECTION( "a negative number of a narrow type" )
	{
		const image_metadata_value value(std::int8_t(-7));

		REQUIRE( value.get_type() == image_metadata_value_type::integer );
		REQUIRE( value.get_integer() == -7 );
	}

	SECTION( "an unsigned number" )
	{
		const image_metadata_value value(std::size_t(4096));

		REQUIRE( value.get_type() == image_metadata_value_type::integer );
		REQUIRE( value.get_integer() == 4096 );
	}

	SECTION( "the two ends of what it holds" )
	{
		const auto lowest = std::numeric_limits<std::int64_t>::min();
		const auto highest = std::numeric_limits<std::int64_t>::max();

		REQUIRE( image_metadata_value(lowest).get_integer() == lowest );
		REQUIRE( image_metadata_value(highest).get_integer() == highest );
		REQUIRE(
			image_metadata_value(
				static_cast<std::uint64_t>(highest)).get_integer() == highest
		);
	}
}

TEST_CASE(
	"image_metadata_value refuses an integer above what it holds",
	"[image_metadata_value]"
)
{
	const auto above =
		static_cast<std::uint64_t>(
			std::numeric_limits<std::int64_t>::max()) + 1;

	REQUIRE_THROWS_AS( image_metadata_value(above), std::out_of_range );
	REQUIRE_THROWS_AS(
		image_metadata_value(std::numeric_limits<std::uint64_t>::max()),
		std::out_of_range
	);
}

TEST_CASE(
	"image_metadata_value is a real when made from a floating point type",
	"[image_metadata_value]"
)
{
	SECTION( "a double" )
	{
		const image_metadata_value value(0.83);

		REQUIRE( value.get_type() == image_metadata_value_type::real );
		REQUIRE( value.get_real() == 0.83 );
	}

	SECTION( "a float" )
	{
		const image_metadata_value value(1.5F);

		REQUIRE( value.get_type() == image_metadata_value_type::real );
		REQUIRE( value.get_real() == 1.5 );
	}

	SECTION( "a long double" )
	{
		const image_metadata_value value(2.25L);

		REQUIRE( value.get_type() == image_metadata_value_type::real );
		REQUIRE( value.get_real() == 2.25 );
	}

	SECTION( "a real of a whole value stays a real" )
	{
		const image_metadata_value value(3.0);

		REQUIRE( value.get_type() == image_metadata_value_type::real );
	}
}

TEST_CASE(
	"image_metadata_value is a text when made from a string",
	"[image_metadata_value]"
)
{
	SECTION( "a std::string" )
	{
		const image_metadata_value value(std::string("Created by vitrio"));

		REQUIRE( value.get_type() == image_metadata_value_type::text );
		REQUIRE( value.get_text() == "Created by vitrio" );
	}

	SECTION( "a string literal" )
	{
		const image_metadata_value value("Created by vitrio");

		REQUIRE( value.get_type() == image_metadata_value_type::text );
		REQUIRE( value.get_text() == "Created by vitrio" );
	}

	SECTION( "an empty string" )
	{
		const image_metadata_value value("");

		REQUIRE( value.get_type() == image_metadata_value_type::text );
		REQUIRE( value.get_text().empty() );
	}

	SECTION( "a text of digits stays a text" )
	{
		const image_metadata_value value("42");

		REQUIRE( value.get_type() == image_metadata_value_type::text );
	}
}

TEST_CASE(
	"image_metadata_value refuses a null string",
	"[image_metadata_value]"
)
{
	const char *const text = nullptr;

	REQUIRE_THROWS_AS( image_metadata_value(text), std::invalid_argument );
}

TEST_CASE(
	"image_metadata_value is got as the type it is and no other",
	"[image_metadata_value]"
)
{
	SECTION( "an integer" )
	{
		const image_metadata_value value(42);

		REQUIRE_THROWS_AS( value.get_real(), std::invalid_argument );
		REQUIRE_THROWS_AS( value.get_text(), std::invalid_argument );
	}

	SECTION( "a real" )
	{
		const image_metadata_value value(0.83);

		REQUIRE_THROWS_AS( value.get_integer(), std::invalid_argument );
		REQUIRE_THROWS_AS( value.get_text(), std::invalid_argument );
	}

	SECTION( "a text" )
	{
		const image_metadata_value value("0.83");

		REQUIRE_THROWS_AS( value.get_integer(), std::invalid_argument );
		REQUIRE_THROWS_AS( value.get_real(), std::invalid_argument );
	}
}

TEST_CASE(
	"image_metadata_value compares by type and by what it holds",
	"[image_metadata_value]"
)
{
	SECTION( "values of one type that hold the same are equal" )
	{
		REQUIRE( image_metadata_value(42) == image_metadata_value(42L) );
		REQUIRE( image_metadata_value(0.5) == image_metadata_value(0.5F) );
		REQUIRE(
			image_metadata_value("text") ==
			image_metadata_value(std::string("text"))
		);
	}

	SECTION( "values of one type that hold another are not" )
	{
		REQUIRE( image_metadata_value(42) != image_metadata_value(43) );
		REQUIRE( image_metadata_value(0.5) != image_metadata_value(0.25) );
		REQUIRE( image_metadata_value("text") != image_metadata_value("") );
	}

	SECTION( "an integer is not the real of the same value" )
	{
		REQUIRE( image_metadata_value(1) != image_metadata_value(1.0) );
	}

	SECTION( "a text is not the number it spells" )
	{
		REQUIRE( image_metadata_value("1") != image_metadata_value(1) );
	}
}

TEST_CASE(
	"image_metadata_value has value semantics",
	"[image_metadata_value]"
)
{
	const image_metadata_value value("a text long enough not to be held "
		"within the string itself");

	SECTION( "a copy equals its source" )
	{
		const image_metadata_value copy(value);

		REQUIRE( copy == value );
	}

	SECTION( "a moved value holds what its source held" )
	{
		image_metadata_value source(value);
		const image_metadata_value moved(std::move(source));

		REQUIRE( moved == value );
	}

	SECTION( "an assignment may change the type" )
	{
		image_metadata_value assigned(42);
		assigned = value;

		REQUIRE( assigned == value );

		assigned = image_metadata_value(0.83);

		REQUIRE( assigned.get_type() == image_metadata_value_type::real );
		REQUIRE( assigned.get_real() == 0.83 );
		REQUIRE( assigned == image_metadata_value(0.83) );
	}
}
