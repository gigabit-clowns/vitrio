// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/file_detector_event_timeline_reader_provider.hpp>

#include "fixtures/format_selector_fixture.hpp"
#include "mock/mock_detector_event_timeline_reader.hpp"

#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/image_file_probe.hpp>

#include <memory>
#include <stdexcept>
#include <trompeloeil.hpp>

using namespace vitrio;

TEST_CASE(
	"a file detector event reader provider needs a format selector",
	"[file_detector_event_timeline_reader_provider]" )
{
	REQUIRE_THROWS_AS(
		file_detector_event_timeline_reader_provider(nullptr),
		std::invalid_argument
	);
}

TEST_CASE_METHOD(
	detector_event_timeline_file_read_format_selector_fixture,
	"a file detector event reader provider opens a file every time it is "
	"asked",
	"[file_detector_event_timeline_reader_provider]" )
{
	auto &format = add_format(image_file_format_suitability::normal);
	file_detector_event_timeline_reader_provider provider(get_selector());

	SECTION( "one request opens the file once" )
	{
		const auto reader =
			std::make_shared<mock_detector_event_timeline_reader>();

		REQUIRE_CALL(format, open(ANY(const image_file_probe&)))
			.LR_WITH( _1.get_path() == "movie.eer" )
			.RETURN(reader);

		REQUIRE( provider.acquire("movie.eer") == reader );
	}

	SECTION( "the same path asked twice is opened twice" )
	{
		REQUIRE_CALL(format, open(ANY(const image_file_probe&)))
			.LR_WITH( _1.get_path() == "movie.eer" )
			.TIMES(2)
			.RETURN(std::make_shared<mock_detector_event_timeline_reader>());

		REQUIRE( provider.acquire("movie.eer") !=
			provider.acquire("movie.eer") );
	}
}

TEST_CASE(
	"a file detector event reader provider reports what the selector reports",
	"[file_detector_event_timeline_reader_provider]" )
{
	file_detector_event_timeline_reader_provider provider(
		std::make_shared<detector_event_timeline_file_read_format_selector>());

	REQUIRE_THROWS_AS( provider.acquire("absent.eer"), image_file_error );
}
