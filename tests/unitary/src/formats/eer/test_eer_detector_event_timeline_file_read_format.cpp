// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/eer/eer_detector_event_timeline_file_read_format.hpp>

#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>
#include <vitrio/image_file_format_suitability.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include "fixtures/eer_test_file.hpp"

#include <fstream>
#include <string>

using namespace vitrio;
using namespace vitrio::eer;
using namespace vitrio::test;

namespace
{

eer_test_movie make_movie()
{
	eer_test_movie movie;
	movie.frames = {{{0, 0, 0, 0}}, {}};
	return movie;
}

} // anonymous namespace

TEST_CASE( "the EER format claims the EER files",
	"[eer_detector_event_timeline_file_read_format]" )
{
	const eer_detector_event_timeline_file_read_format format;

	SECTION( "it is named" )
	{
		REQUIRE( format.get_name() == "EER" );
	}

	SECTION( "a TIFF file of the extension is claimed" )
	{
		const scoped_path path("eer_format_claimed.eer");
		write_eer_file(path.get(), make_movie());

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::normal );
	}

	SECTION( "a TIFF file of another extension is not" )
	{
		const scoped_path path("eer_format_other.tif");
		write_eer_file(path.get(), make_movie());

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::unsupported );
	}

	SECTION( "a file of the extension that is not TIFF is not" )
	{
		const scoped_path path("eer_format_not_tiff.eer");
		std::ofstream(path.get().c_str(), std::ios::binary) << "not a tiff";

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::unsupported );
	}
}

TEST_CASE( "the EER format opens a file as a reader of its events",
	"[eer_detector_event_timeline_file_read_format]" )
{
	const scoped_path path("eer_format_opened.eer");
	write_eer_file(path.get(), make_movie());

	SECTION( "through the format" )
	{
		const eer_detector_event_timeline_file_read_format format;
		const auto reader = format.open(image_file_probe(path.get()));

		REQUIRE( reader != nullptr );
		REQUIRE( reader->get_descriptor().get_time_extent() == 2 );
	}

	SECTION( "through the bundled formats" )
	{
		const auto &formats =
			detector_event_timeline_file_read_format_selector::get_shared();
		const auto *chosen = formats->get_most_suitable_format(
			image_file_probe(path.get()));

		REQUIRE( chosen != nullptr );
		REQUIRE( chosen->get_name() == "EER" );
		REQUIRE( formats->open(path.get())->get_descriptor()
			.get_time_extent() == 2 );
	}
}
