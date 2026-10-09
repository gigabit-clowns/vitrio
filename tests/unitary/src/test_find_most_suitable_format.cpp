// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <find_most_suitable_format.hpp>

#include <cstddef>
#include <vector>

using namespace vitrio;

namespace
{

image_format_suitability identity(image_format_suitability suitability)
{
	return suitability;
}

} // anonymous namespace

TEST_CASE(
	"find_most_suitable_format finds the item of the highest suitability",
	"[find_most_suitable_format]"
)
{
	const std::vector<image_format_suitability> items = {
		image_format_suitability::fallback,
		image_format_suitability::optimal,
		image_format_suitability::normal,
	};

	const auto found =
		find_most_suitable_format(items.begin(), items.end(), identity);

	REQUIRE( found == items.begin() + 1 );
}

TEST_CASE(
	"find_most_suitable_format finds the first of the items that tie",
	"[find_most_suitable_format]"
)
{
	const std::vector<image_format_suitability> items = {
		image_format_suitability::fallback,
		image_format_suitability::normal,
		image_format_suitability::normal,
	};

	const auto found =
		find_most_suitable_format(items.begin(), items.end(), identity);

	REQUIRE( found == items.begin() + 1 );
}

TEST_CASE(
	"find_most_suitable_format finds nothing where nothing is supported",
	"[find_most_suitable_format]"
)
{
	SECTION( "in a range of unsupported items" )
	{
		const std::vector<image_format_suitability> items = {
			image_format_suitability::unsupported,
			image_format_suitability::unsupported,
		};

		const auto found =
			find_most_suitable_format(items.begin(), items.end(), identity);

		REQUIRE( found == items.end() );
	}

	SECTION( "in an empty range" )
	{
		const std::vector<image_format_suitability> items;

		const auto found =
			find_most_suitable_format(items.begin(), items.end(), identity);

		REQUIRE( found == items.end() );
	}
}

TEST_CASE(
	"find_most_suitable_format evaluates each item once",
	"[find_most_suitable_format]"
)
{
	const std::vector<image_format_suitability> items = {
		image_format_suitability::normal,
		image_format_suitability::unsupported,
		image_format_suitability::optimal,
	};
	std::size_t evaluations = 0;

	find_most_suitable_format(
		items.begin(),
		items.end(),
		[&evaluations] (image_format_suitability suitability)
		{
			++evaluations;
			return suitability;
		}
	);

	REQUIRE( evaluations == items.size() );
}
