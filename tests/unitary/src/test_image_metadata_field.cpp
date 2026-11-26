// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_metadata_field.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

using namespace vitrio;

namespace
{

constexpr image_metadata_field<std::int64_t> frames{"application.frames"};
constexpr image_metadata_field<double> dose{"application.dose"};
constexpr image_metadata_field<std::string> session{"application.session"};

// A standard field, declared here as image_metadata_fields declares it.
constexpr image_metadata_field<double> pixel_size_x{"pixel_size_x"};

// A value of the wrong type for a field does not compile, which a test can
// only tell from whether the call is well formed.
template <typename Field, typename Value, typename = void>
struct is_settable : std::false_type
{
};

template <typename Field, typename Value>
struct is_settable<
	Field,
	Value,
	decltype(void(set_field(
		std::declval<image_metadata&>(),
		std::declval<Field>(),
		std::declval<Value>()
	)))
> : std::true_type
{
};

using integer_field = image_metadata_field<std::int64_t>;
using real_field = image_metadata_field<double>;
using text_field = image_metadata_field<std::string>;

static_assert( is_settable<real_field, double>::value, "" );
static_assert( is_settable<real_field, float>::value, "" );
static_assert( is_settable<real_field, int>::value, "" );
static_assert( is_settable<real_field, std::size_t>::value, "" );
static_assert( !is_settable<real_field, bool>::value, "" );
static_assert( !is_settable<real_field, const char*>::value, "" );
static_assert( !is_settable<real_field, std::string>::value, "" );
static_assert( !is_settable<real_field, std::nullptr_t>::value, "" );

static_assert( is_settable<integer_field, int>::value, "" );
static_assert( is_settable<integer_field, std::uint64_t>::value, "" );
static_assert( !is_settable<integer_field, double>::value, "" );
static_assert( !is_settable<integer_field, float>::value, "" );
static_assert( !is_settable<integer_field, bool>::value, "" );
static_assert( !is_settable<integer_field, const char*>::value, "" );

static_assert( is_settable<text_field, std::string>::value, "" );
static_assert( is_settable<text_field, const char*>::value, "" );
static_assert( !is_settable<text_field, int>::value, "" );
static_assert( !is_settable<text_field, double>::value, "" );
static_assert( !is_settable<text_field, bool>::value, "" );
static_assert( !is_settable<text_field, std::nullptr_t>::value, "" );

static_assert(
	std::is_same<integer_field::value_type, std::int64_t>::value,
	"A field names the type of its value"
);
static_assert(
	std::is_same<
		decltype(get_field(std::declval<image_metadata>(), dose)),
		const double&
	>::value,
	"A field is got as its type"
);
static_assert(
	std::is_same<
		decltype(find_field(std::declval<image_metadata>(), session)),
		const std::string*
	>::value,
	"A field is found as its type"
);

} // anonymous namespace

TEST_CASE(
	"image_metadata_field holds the label it was constructed from",
	"[image_metadata_field]"
)
{
	REQUIRE( std::string(frames.get_label()) == "application.frames" );
	REQUIRE( std::string(dose.get_label()) == "application.dose" );
	REQUIRE( std::string(session.get_label()) == "application.session" );
}

TEST_CASE(
	"set_field states a value under the label of the field",
	"[image_metadata_field]"
)
{
	image_metadata metadata;

	SECTION( "an integer" )
	{
		set_field(metadata, frames, 40);

		REQUIRE(
			metadata.get("application.frames") == image_metadata_value(40)
		);
	}

	SECTION( "a real" )
	{
		set_field(metadata, dose, 1.25);

		REQUIRE(
			metadata.get("application.dose") == image_metadata_value(1.25)
		);
	}

	SECTION( "a text, from a string and from a literal" )
	{
		set_field(metadata, session, std::string("krios-3"));
		REQUIRE(
			metadata.get("application.session") ==
			image_metadata_value("krios-3")
		);

		set_field(metadata, session, "krios-4");
		REQUIRE(
			metadata.get("application.session") ==
			image_metadata_value("krios-4")
		);
	}

	SECTION( "setting it again replaces the value" )
	{
		set_field(metadata, frames, 40);
		set_field(metadata, frames, 50);

		REQUIRE( metadata.get_entries().size() == 1 );
		REQUIRE( get_field(metadata, frames) == 50 );
	}
}

TEST_CASE(
	"set_field holds an integer given for a real field as a real",
	"[image_metadata_field]"
)
{
	image_metadata metadata;

	SECTION( "under a label with a prefix, which the metadata does not "
		"know the type of" )
	{
		set_field(metadata, dose, 2);

		const auto &value = metadata.get("application.dose");
		REQUIRE( value.get_type() == image_metadata_value_type::real );
		REQUIRE( value.get_real() == 2.0 );
	}

	SECTION( "under a standard field" )
	{
		set_field(metadata, pixel_size_x, 2);

		REQUIRE( get_field(metadata, pixel_size_x) == 2.0 );
	}
}

TEST_CASE(
	"set_field refuses what the metadata refuses",
	"[image_metadata_field]"
)
{
	image_metadata metadata;

	SECTION( "a field with no prefix that is no standard one" )
	{
		constexpr image_metadata_field<double> misspelled{"pixelsize_x"};

		REQUIRE_THROWS_AS(
			set_field(metadata, misspelled, 0.83),
			std::invalid_argument
		);
	}

	SECTION( "a field declared for a standard label with another type" )
	{
		constexpr image_metadata_field<std::string> mistyped{"pixel_size_x"};

		REQUIRE_THROWS_AS(
			set_field(metadata, mistyped, "0.83"),
			std::invalid_argument
		);
	}

	SECTION( "an integer above what the metadata holds" )
	{
		REQUIRE_THROWS_AS(
			set_field(metadata, frames, ~std::uint64_t(0)),
			std::out_of_range
		);
	}

	REQUIRE( metadata.get_entries().empty() );
}

TEST_CASE(
	"find_field finds the value of a field, or nothing",
	"[image_metadata_field]"
)
{
	image_metadata metadata;

	SECTION( "nothing when the metadata does not state it" )
	{
		REQUIRE( find_field(metadata, frames) == nullptr );
		REQUIRE( find_field(metadata, dose) == nullptr );
		REQUIRE( find_field(metadata, session) == nullptr );
	}

	SECTION( "the value when it does" )
	{
		set_field(metadata, frames, 40);
		set_field(metadata, dose, 1.25);
		set_field(metadata, session, "krios-3");

		const auto *found_frames = find_field(metadata, frames);
		const auto *found_dose = find_field(metadata, dose);
		const auto *found_session = find_field(metadata, session);

		REQUIRE( found_frames != nullptr );
		REQUIRE( *found_frames == 40 );
		REQUIRE( found_dose != nullptr );
		REQUIRE( *found_dose == 1.25 );
		REQUIRE( found_session != nullptr );
		REQUIRE( *found_session == "krios-3" );
	}

	SECTION( "the value the metadata holds, and not a copy of it" )
	{
		set_field(metadata, dose, 1.25);

		REQUIRE(
			find_field(metadata, dose) ==
			&metadata.get("application.dose").get_real()
		);
	}
}

TEST_CASE(
	"get_field gets the value of a field, or refuses",
	"[image_metadata_field]"
)
{
	image_metadata metadata;
	set_field(metadata, frames, 40);
	set_field(metadata, session, "krios-3");

	REQUIRE( get_field(metadata, frames) == 40 );
	REQUIRE( get_field(metadata, session) == "krios-3" );
	REQUIRE_THROWS_AS( get_field(metadata, dose), std::out_of_range );
}

TEST_CASE(
	"A field refuses a value of another type than its own",
	"[image_metadata_field]"
)
{
	// Only a label with a prefix can hold another type than a field
	// declared for it expects.
	image_metadata metadata;
	metadata.set("application.dose", 2);
	metadata.set("application.frames", "forty");
	metadata.set("application.session", 3.5);

	REQUIRE_THROWS_AS( find_field(metadata, dose), std::invalid_argument );
	REQUIRE_THROWS_AS( get_field(metadata, dose), std::invalid_argument );
	REQUIRE_THROWS_AS( find_field(metadata, frames), std::invalid_argument );
	REQUIRE_THROWS_AS( get_field(metadata, frames), std::invalid_argument );
	REQUIRE_THROWS_AS( find_field(metadata, session), std::invalid_argument );
	REQUIRE_THROWS_AS( get_field(metadata, session), std::invalid_argument );
}
