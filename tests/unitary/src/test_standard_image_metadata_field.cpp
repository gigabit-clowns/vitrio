// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <standard_image_metadata_field.hpp>

#include <vitrio/image_metadata_fields.hpp>

#include <cstdint>
#include <set>
#include <string>

using namespace vitrio;

TEST_CASE(
	"standard_image_metadata_field takes its label and its type from a field",
	"[standard_image_metadata_field]"
)
{
	SECTION( "an integer" )
	{
		constexpr image_metadata_field<std::int64_t> field{"frames"};
		constexpr standard_image_metadata_field standard(field);

		REQUIRE( std::string(standard.get_label()) == "frames" );
		REQUIRE( standard.get_type() == image_metadata_value_type::integer );
	}

	SECTION( "a real" )
	{
		constexpr image_metadata_field<double> field{"dose"};
		constexpr standard_image_metadata_field standard(field);

		REQUIRE( std::string(standard.get_label()) == "dose" );
		REQUIRE( standard.get_type() == image_metadata_value_type::real );
	}

	SECTION( "a text" )
	{
		constexpr image_metadata_field<std::string> field{"session"};
		constexpr standard_image_metadata_field standard(field);

		REQUIRE( std::string(standard.get_label()) == "session" );
		REQUIRE( standard.get_type() == image_metadata_value_type::text );
	}
}

TEST_CASE(
	"The standard fields have labels with no dot, each its own",
	"[standard_image_metadata_field]"
)
{
	const auto fields = get_standard_image_metadata_fields();
	REQUIRE_FALSE( fields.empty() );

	std::set<std::string> labels;
	for (const auto &field : fields)
	{
		const std::string label(field.get_label());

		CHECK_FALSE( label.empty() );
		CHECK( label.find('.') == std::string::npos );
		CHECK( labels.insert(label).second );
	}
}

TEST_CASE(
	"find_standard_image_metadata_field finds a field by its label",
	"[standard_image_metadata_field]"
)
{
	SECTION( "every standard field" )
	{
		for (const auto &field : get_standard_image_metadata_fields())
		{
			REQUIRE(
				find_standard_image_metadata_field(field.get_label()) ==
				&field
			);
		}
	}

	SECTION( "with the type its constant states" )
	{
		const auto *field = find_standard_image_metadata_field(
			image_metadata_fields::pixel_size_x.get_label());

		REQUIRE( field != nullptr );
		REQUIRE( field->get_type() == image_metadata_value_type::real );
	}

	SECTION( "nothing for any other label" )
	{
		REQUIRE( find_standard_image_metadata_field("") == nullptr );
		REQUIRE( find_standard_image_metadata_field("pixel_size") == nullptr );
		REQUIRE(
			find_standard_image_metadata_field("pixel_size_xy") == nullptr
		);
		REQUIRE(
			find_standard_image_metadata_field("Pixel_Size_X") == nullptr
		);
		REQUIRE( find_standard_image_metadata_field("mrc.labels") == nullptr );
	}
}
