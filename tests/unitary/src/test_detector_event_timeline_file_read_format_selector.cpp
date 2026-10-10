// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>

#include <vitrio/detector_event_timeline_file_read_format.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include "fixtures/format_selector_fixture.hpp"
#include "mock/mock_detector_event_timeline_file_read_format.hpp"
#include "mock/mock_detector_event_timeline_reader.hpp"

#include <fstream>
#include <memory>
#include <string>
#include <trompeloeil.hpp>
#include <utility>

using namespace vitrio;

namespace
{

auto names(const std::string &path)
{
	return Catch::Matchers::MessageMatches(
		Catch::Matchers::StartsWith(path + ": ")
	);
}

} // anonymous namespace

TEST_CASE(
	"an empty detector event format selector recognizes nothing",
	"[detector_event_timeline_file_read_format_selector]" )
{
	const detector_event_timeline_file_read_format_selector selector;

	SECTION( "no format claims a file" )
	{
		REQUIRE( selector.get_most_suitable_format(
			image_file_probe("absent.eer")) == nullptr );
	}

	SECTION( "opening a path no file is at reports it missing" )
	{
		REQUIRE_THROWS_MATCHES(
			selector.open("absent.eer"),
			image_file_error,
			names("absent.eer")
		);
	}

	SECTION( "opening a file that is there reports it unsupported" )
	{
		const scoped_path path("event_format_selector_unclaimed.bin");
		std::ofstream(path.get().c_str(), std::ios::binary).put('\0');

		REQUIRE_THROWS_MATCHES(
			selector.open(path.get()),
			unsupported_operation_error,
			names(path.get())
		);
	}
}

TEST_CASE_METHOD(
	detector_event_timeline_file_read_format_selector_fixture,
	"the detector event format selector picks the most suitable format",
	"[detector_event_timeline_file_read_format_selector]" )
{
	const auto &selector = *get_selector();
	const image_file_probe probe("absent.eer");

	SECTION( "the only supporting format is chosen" )
	{
		const auto &only = add_format(image_file_format_suitability::normal);

		REQUIRE( selector.get_most_suitable_format(probe) == &only );
	}

	SECTION( "the highest priority wins" )
	{
		add_format(image_file_format_suitability::fallback);
		const auto &optimal =
			add_format(image_file_format_suitability::optimal);
		add_format(image_file_format_suitability::normal);

		REQUIRE( selector.get_most_suitable_format(probe) == &optimal );
	}

	SECTION( "every format declining leaves nothing suitable" )
	{
		add_format(image_file_format_suitability::unsupported);

		REQUIRE( selector.get_most_suitable_format(probe) == nullptr );
		REQUIRE_THROWS_AS( selector.open("absent.eer"), image_file_error );
	}

	SECTION( "the chosen format opens the file" )
	{
		auto &only = add_format(image_file_format_suitability::normal);
		const auto reader =
			std::make_shared<mock_detector_event_timeline_reader>();

		REQUIRE_CALL(only, open(ANY(const image_file_probe&)))
			.LR_WITH( _1.get_path() == "absent.eer" )
			.RETURN(reader);

		REQUIRE( selector.open("absent.eer") == reader );
	}
}

TEST_CASE(
	"the detector event format selector refuses a null format",
	"[detector_event_timeline_file_read_format_selector]" )
{
	detector_event_timeline_file_read_format_selector selector;

	REQUIRE_FALSE( selector.register_format(nullptr) );
	REQUIRE( selector.register_format(
		std::make_unique<mock_detector_event_timeline_file_read_format>()) );
}

TEST_CASE(
	"the shared detector event format selector is one instance",
	"[detector_event_timeline_file_read_format_selector]" )
{
	const auto &shared =
		detector_event_timeline_file_read_format_selector::get_shared();

	REQUIRE( shared != nullptr );
	REQUIRE( shared ==
		detector_event_timeline_file_read_format_selector::get_shared() );
}
