// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/eer/eer_detector_event_timeline_reader.hpp>

#include <vitrio/detector_event_timeline.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/tests/scoped_path.hpp>

#include "fixtures/eer_test_file.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

using namespace vitrio;
using namespace vitrio::eer;
using namespace vitrio::test;

namespace
{

using extents = std::vector<std::size_t>;
using positions = std::vector<std::uint32_t>;

extents to_vector(span<const std::size_t> values)
{
	return extents(values.begin(), values.end());
}

positions to_vector(detector_event_position_view view)
{
	const auto values = view.get_coordinates();
	return positions(values.begin(), values.end());
}

// Three frames of 8 by 6 pixels in strips of 3 rows, the second one empty
// and the third with an event in each strip.
eer_test_movie make_movie()
{
	eer_test_movie movie;
	movie.frames = {
		{{0, 0, 0, 0}, {2, 7, 3, 1}},
		{},
		{{1, 4, 2, 2}, {5, 1, 0, 3}},
	};
	return movie;
}

} // anonymous namespace

TEST_CASE( "an EER file is described as the grid its subpixels make",
	"[eer_detector_event_timeline_reader]" )
{
	const scoped_path path("eer_reader_described.eer");
	write_eer_file(path.get(), make_movie());

	const eer_detector_event_timeline_reader reader(path.get());
	const auto &descriptor = reader.get_descriptor();

	REQUIRE( to_vector(descriptor.get_extents()) == extents{24, 32} );
	REQUIRE( to_vector(descriptor.get_subdivisions()) == extents{4, 4} );
	REQUIRE( descriptor.get_time_extent() == 3 );
	REQUIRE( descriptor.get_time_quantum() == 0.0 );
}

TEST_CASE( "an EER file is read a stretch of frames at a time",
	"[eer_detector_event_timeline_reader]" )
{
	const scoped_path path("eer_reader_frames.eer");
	write_eer_file(path.get(), make_movie());

	const eer_detector_event_timeline_reader reader(path.get());
	detector_event_timeline events(2);

	SECTION( "every frame, a group per frame that counted anything" )
	{
		reader.read(0, 3, events);

		REQUIRE( events.get_group_count() == 2 );
		REQUIRE( events.get_timestamp(0) == 0 );
		REQUIRE( events.get_timestamp(1) == 2 );
		REQUIRE( to_vector(events.get_positions(0)) == positions{
			0, 0,
			(2 << 2) | 1, (7 << 2) | 3
		} );
		REQUIRE( to_vector(events.get_positions(1)) == positions{
			(1 << 2) | 2, (4 << 2) | 2,
			(5 << 2) | 3, (1 << 2) | 0
		} );
	}

	SECTION( "a stretch replaces what the timeline held" )
	{
		reader.read(0, 1, events);
		reader.read(2, 3, events);

		REQUIRE( events.get_group_count() == 1 );
		REQUIRE( events.get_timestamp(0) == 2 );
	}

	SECTION( "a stretch of no frames reads nothing" )
	{
		reader.read(1, 1, events);

		REQUIRE( events.get_group_count() == 0 );
	}

	SECTION( "a stretch past the last frame is refused" )
	{
		REQUIRE_THROWS_AS( reader.read(2, 4, events), std::out_of_range );
	}

	SECTION( "a stretch that ends before it begins is refused" )
	{
		REQUIRE_THROWS_AS(
			reader.read(2, 1, events),
			std::invalid_argument
		);
	}

	SECTION( "a timeline of another rank is refused" )
	{
		detector_event_timeline other(3);

		REQUIRE_THROWS_AS(
			reader.read(0, 1, other),
			std::invalid_argument
		);
	}
}

TEST_CASE( "an EER file of variable widths is read with them",
	"[eer_detector_event_timeline_reader]" )
{
	const scoped_path path("eer_reader_variable.eer");
	auto movie = make_movie();
	movie.compression = 65002;
	movie.rle_bits = 3;
	movie.horizontal_bits = 1;
	movie.vertical_bits = 3;
	movie.states_widths = true;
	movie.frames = {{{0, 0, 1, 7}, {5, 7, 0, 2}}};
	write_eer_file(path.get(), movie);

	const eer_detector_event_timeline_reader reader(path.get());
	detector_event_timeline events(2);
	reader.read(0, 1, events);

	REQUIRE( to_vector(reader.get_descriptor().get_extents()) ==
		extents{48, 16} );
	REQUIRE( to_vector(events.get_positions(0)) == positions{
		7, 1,
		(5 << 3) | 2, (7 << 1) | 0
	} );
}

TEST_CASE( "an EER file that is not one is refused",
	"[eer_detector_event_timeline_reader]" )
{
	const scoped_path path("eer_reader_refused.eer");

	SECTION( "a TIFF file compressed otherwise" )
	{
		auto movie = make_movie();
		movie.compression = 1;
		write_eer_file(path.get(), movie);

		REQUIRE_THROWS_AS(
			eer_detector_event_timeline_reader(path.get()),
			image_file_format_error
		);
	}

	SECTION( "a frame of another size than the first" )
	{
		auto pages = make_eer_pages(make_movie());
		pages[2].width = 4;
		write_eer_pages(path.get(), pages);

		const eer_detector_event_timeline_reader reader(path.get());
		detector_event_timeline events(2);

		REQUIRE_NOTHROW( reader.read(0, 2, events) );
		REQUIRE_THROWS_AS(
			reader.read(0, 3, events),
			image_file_format_error
		);
	}

	SECTION( "a frame of another encoding than the first" )
	{
		auto pages = make_eer_pages(make_movie());
		pages[1].compression = 65000;
		write_eer_pages(path.get(), pages);

		const eer_detector_event_timeline_reader reader(path.get());
		detector_event_timeline events(2);

		REQUIRE_THROWS_AS(
			reader.read(1, 2, events),
			image_file_format_error
		);
	}

	SECTION( "a strip whose runs pass its last pixel" )
	{
		auto pages = make_eer_pages(make_movie());
		eer_bit_writer stream;
		stream.write(30, 11);
		pages[0].strips[1] = stream.finish();
		write_eer_pages(path.get(), pages);

		// A strip of 3 rows of 8 pixels holds 24 of them, and the run is
		// of 30.
		const eer_detector_event_timeline_reader reader(path.get());
		detector_event_timeline events(2);

		REQUIRE_THROWS_AS(
			reader.read(0, 1, events),
			image_file_format_error
		);
	}
}
