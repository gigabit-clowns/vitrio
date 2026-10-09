// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/file_image_reader_provider.hpp>

#include "fixtures/format_selector_fixture.hpp"
#include "mock/mock_image_reader.hpp"

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_file_read_format_selector.hpp>

#include <memory>
#include <stdexcept>
#include <trompeloeil.hpp>

using namespace vitrio;

TEST_CASE( "a file reader provider needs a format selector",
	"[file_image_reader_provider]" )
{
	REQUIRE_THROWS_AS(
		file_image_reader_provider(nullptr),
		std::invalid_argument
	);
}

TEST_CASE_METHOD(
	read_format_selector_fixture,
	"a file reader provider opens a file every time it is asked",
	"[file_image_reader_provider]"
)
{
	auto &format = add_format(image_file_format_suitability::normal);
	file_image_reader_provider provider(get_selector());

	SECTION( "one request opens the file once" )
	{
		const auto reader = std::make_shared<mock_image_reader>();

		REQUIRE_CALL(format, open(ANY(const image_file_probe&)))
			.LR_WITH( _1.get_path() == "stack_0.mrcs" )
			.RETURN(reader);

		REQUIRE( provider.acquire("stack_0.mrcs") == reader );
	}

	SECTION( "the same path asked twice is opened twice" )
	{
		// It keeps nothing, which is what makes it the plain one.
		REQUIRE_CALL(format, open(ANY(const image_file_probe&)))
			.LR_WITH( _1.get_path() == "stack_0.mrcs" )
			.TIMES(2)
			.RETURN(std::make_shared<mock_image_reader>());

		const auto first = provider.acquire("stack_0.mrcs");
		const auto second = provider.acquire("stack_0.mrcs");

		REQUIRE( first != second );
	}

	SECTION( "distinct paths are opened separately" )
	{
		REQUIRE_CALL(format, open(ANY(const image_file_probe&)))
			.LR_WITH( _1.get_path() == "stack_0.mrcs" )
			.RETURN(std::make_shared<mock_image_reader>());
		REQUIRE_CALL(format, open(ANY(const image_file_probe&)))
			.LR_WITH( _1.get_path() == "stack_1.mrcs" )
			.RETURN(std::make_shared<mock_image_reader>());

		provider.acquire("stack_0.mrcs");
		provider.acquire("stack_1.mrcs");
	}
}

TEST_CASE(
	"a file reader provider reports what the selector reports",
	"[file_image_reader_provider]"
)
{
	// A selector with no format recognizes nothing, and the provider adds
	// no opinion of its own.
	file_image_reader_provider provider(
		std::make_shared<image_file_read_format_selector>()
	);

	REQUIRE_THROWS_AS(
		provider.acquire("stack_0.mrcs"),
		image_file_error
	);
}
