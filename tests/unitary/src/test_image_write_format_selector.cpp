// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <vitrio/image_write_format_selector.hpp>

#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_probe.hpp>
#include <vitrio/image_write_format.hpp>

#include "fixtures/format_selector_fixture.hpp"
#include "mock/mock_image_write_format.hpp"
#include "mock/mock_image_writer.hpp"

#include <cstddef>
#include <memory>
#include <trompeloeil.hpp>
#include <vector>

using namespace vitrio;

namespace
{

const std::vector<std::size_t> file_extents = {2, 3, 4};
const image_descriptor file_descriptor(
	make_span(file_extents),
	2,
	numerical_type::int16
);

} // anonymous namespace

TEST_CASE( "an empty write format selector recognizes nothing",
	"[image_write_format_selector]" )
{
	const image_write_format_selector selector;

	SECTION( "no format claims a file" )
	{
		REQUIRE( selector.get_most_suitable_format(
			image_probe("absent.mrc")) == nullptr );
	}

	SECTION( "opening reports that nothing is suitable" )
	{
		REQUIRE_THROWS_MATCHES(
			selector.open("absent.mrc", file_descriptor, image_metadata()),
			unsupported_operation_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith("absent.mrc: ")
			)
		);
	}
}

TEST_CASE_METHOD(
	write_format_selector_fixture,
	"the write format selector picks the most suitable format",
	"[image_write_format_selector]"
)
{
	const auto &selector = *get_selector();
	const image_probe probe("absent.mrc");

	SECTION( "the highest priority wins" )
	{
		add_format(image_format_suitability::normal);
		const auto &optimal = add_format(image_format_suitability::optimal);

		REQUIRE( selector.get_most_suitable_format(probe) == &optimal );
	}

	SECTION( "a file that does not exist yet is still decided on" )
	{
		add_format(image_format_suitability::normal);

		REQUIRE_FALSE( probe.exists() );
		REQUIRE( selector.get_most_suitable_format(probe) != nullptr );
	}

	SECTION( "the chosen format is handed what the file is to be" )
	{
		auto &only = add_format(image_format_suitability::normal);
		const auto writer = std::make_shared<mock_image_writer>();

		REQUIRE_CALL(
			only,
			open(trompeloeil::_, trompeloeil::_, trompeloeil::_)
		)
			.LR_WITH( _1.get_path() == "absent.mrc" && _2 == file_descriptor )
			.RETURN(writer);

		REQUIRE(
			selector.open("absent.mrc", file_descriptor, image_metadata()) ==
			writer
		);
	}
}

TEST_CASE( "the write format selector refuses a null format",
	"[image_write_format_selector]" )
{
	image_write_format_selector selector;

	REQUIRE_FALSE( selector.register_format(nullptr) );
	REQUIRE( selector.register_format(
		std::make_unique<mock_image_write_format>()) );
}

TEST_CASE(
	"register_builtin_formats adds the write formats bundled with the library",
	"[image_write_format_selector]"
)
{
	image_write_format_selector selector;
	const auto probe = image_probe("absent.mrc");

	REQUIRE( selector.get_most_suitable_format(probe) == nullptr );

	selector.register_builtin_formats();

	const auto *chosen = selector.get_most_suitable_format(probe);

	REQUIRE( chosen != nullptr );
	REQUIRE( chosen->get_name() == "MRC" );
}

TEST_CASE(
	"the shared write format selector holds the bundled formats",
	"[image_write_format_selector]"
)
{
	const auto &shared = image_write_format_selector::get_shared();
	const auto probe = image_probe("absent.mrc");

	SECTION( "it is not null" )
	{
		REQUIRE( shared != nullptr );
	}

	SECTION( "it is the same selector in every call" )
	{
		REQUIRE( shared == image_write_format_selector::get_shared() );
	}

	SECTION( "it selects a bundled format" )
	{
		const auto *chosen = shared->get_most_suitable_format(probe);

		REQUIRE( chosen != nullptr );
		REQUIRE( chosen->get_name() == "MRC" );
	}
}
