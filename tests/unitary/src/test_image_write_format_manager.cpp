// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <vitrio/image_write_format_manager.hpp>

#include "fixtures/format_manager_fixture.hpp"
#include "mock/mock_image_writer.hpp"
#include "mock/mock_image_write_format.hpp"

#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_probe.hpp>

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

TEST_CASE( "an empty write manager recognizes nothing",
	"[image_write_format_manager]" )
{
	const image_write_format_manager manager;

	SECTION( "no format claims a file" )
	{
		REQUIRE( manager.get_most_suitable_format(
			image_probe("absent.mrc")) == nullptr );
	}

	SECTION( "opening reports that nothing is suitable" )
	{
		REQUIRE_THROWS_MATCHES(
			manager.open("absent.mrc", file_descriptor, image_metadata()),
			unsupported_operation_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith("absent.mrc: ")
			)
		);
	}
}

TEST_CASE_METHOD(
	write_format_manager_fixture,
	"the write manager picks the most suitable format",
	"[image_write_format_manager]"
)
{
	const auto &manager = *get_manager();
	const image_probe probe("absent.mrc");

	SECTION( "the highest priority wins" )
	{
		add_format(image_format_suitability::normal);
		const auto &optimal = add_format(image_format_suitability::optimal);

		REQUIRE( manager.get_most_suitable_format(probe) == &optimal );
	}

	SECTION( "a file that does not exist yet is still decided on" )
	{
		add_format(image_format_suitability::normal);

		REQUIRE_FALSE( probe.exists() );
		REQUIRE( manager.get_most_suitable_format(probe) != nullptr );
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
			manager.open("absent.mrc", file_descriptor, image_metadata()) ==
			writer
		);
	}
}

TEST_CASE( "the write manager refuses a null format",
	"[image_write_format_manager]" )
{
	image_write_format_manager manager;

	REQUIRE_FALSE( manager.register_format(nullptr) );
	REQUIRE( manager.register_format(
		std::make_unique<mock_image_write_format>()) );
}
