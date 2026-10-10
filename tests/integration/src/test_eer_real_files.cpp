// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/counting_detector_event_renderer.hpp>
#include <vitrio/detector_event_fractionation.hpp>
#include <vitrio/detector_event_image_reader_provider.hpp>
#include <vitrio/detector_event_timeline.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>
#include <vitrio/detector_event_timeline_reader.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/file_detector_event_timeline_reader_provider.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_read.hpp>
#include <vitrio/tests/assets.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace vitrio;

namespace
{

using extents = std::vector<std::size_t>;
using counts = std::vector<std::uint32_t>;

/**
 * One of the files of tests/assets/eer, as its README describes it.
 */
struct eer_asset
{
	std::string name;
	std::size_t frame_count;
	std::size_t width;
	std::size_t height;
	std::size_t horizontal_bits;
	std::size_t vertical_bits;
	std::set<std::size_t> empty_frames;
};

const eer_asset rle7 = {"movie_rle7.eer", 4, 8, 6, 2, 2, {}};
const eer_asset rle6 = {"movie_rle6.eer", 3, 16, 8, 1, 2, {1}};

// The events of each frame on the grid of subpixels, restated from the
// formula of the README rather than read off the file by other means.
std::vector<counts> expected_frames(const eer_asset &asset)
{
	const auto columns = asset.width << asset.horizontal_bits;
	const auto rows = asset.height << asset.vertical_bits;

	std::vector<counts> frames;
	for (std::size_t frame = 0; frame < asset.frame_count; ++frame)
	{
		counts image(rows * columns, 0);
		if (asset.empty_frames.count(frame) == 0)
		{
			for (std::size_t pixel = 0;
				pixel < asset.width * asset.height;
				++pixel)
			{
				if ((7 * pixel + 3 * frame) % 5 != 0)
				{
					continue;
				}
				const auto horizontal = (pixel + frame) %
					(std::size_t(1) << asset.horizontal_bits);
				const auto vertical = (3 * pixel + frame) %
					(std::size_t(1) << asset.vertical_bits);
				const auto row =
					((pixel / asset.width) << asset.vertical_bits) + vertical;
				const auto column =
					((pixel % asset.width) << asset.horizontal_bits) +
					horizontal;
				++image[row * columns + column];
			}
		}
		frames.push_back(image);
	}

	return frames;
}

// The frames from first up to last added up.
counts add_frames(
	const std::vector<counts> &frames,
	std::size_t first,
	std::size_t last
)
{
	counts sum(frames.front().size(), 0);
	for (auto frame = first; frame < last; ++frame)
	{
		for (std::size_t i = 0; i < sum.size(); ++i)
		{
			sum[i] += frames[frame][i];
		}
	}
	return sum;
}

std::shared_ptr<detector_event_timeline_reader_provider> make_events()
{
	return std::make_shared<file_detector_event_timeline_reader_provider>(
		detector_event_timeline_file_read_format_selector::get_shared());
}

array read_movie(
	const eer_asset &asset,
	std::size_t fraction_count,
	const extents &bin
)
{
	detector_event_image_reader_provider readers(
		make_events(),
		detector_event_fractionation(fraction_count),
		std::make_shared<counting_detector_event_renderer>(make_span(bin))
	);
	return read(get_eer_asset_path(asset.name), readers);
}

extents extents_of(const array &values)
{
	const auto read = values.get_descriptor().get_extents();
	return extents(read.begin(), read.end());
}

counts values_of(const array &values, std::size_t first, std::size_t count)
{
	const auto *data =
		reinterpret_cast<const std::uint32_t*>(values.get_data()) + first;
	return counts(data, data + count);
}

} // anonymous namespace

TEST_CASE(
	"EER files vitrio did not write are read as the events they hold",
	"[eer][detector_event_timeline_file_read_format_selector]" )
{
	const auto events = make_events();

	SECTION( "an encoding of fixed widths" )
	{
		const auto reader =
			events->acquire(get_eer_asset_path(rle7.name));
		const auto &descriptor = reader->get_descriptor();
		const auto grid = descriptor.get_extents();
		const auto subdivisions = descriptor.get_subdivisions();

		REQUIRE( extents(grid.begin(), grid.end()) == extents{24, 32} );
		REQUIRE( extents(subdivisions.begin(), subdivisions.end()) ==
			extents{4, 4} );
		REQUIRE( descriptor.get_time_extent() == 4 );

		detector_event_timeline timeline(2);
		reader->read(0, 4, timeline);

		REQUIRE( timeline.get_group_count() == 4 );
		REQUIRE( timeline.get_event_count() == 39 );
	}

	SECTION( "an encoding whose widths its tags state" )
	{
		const auto reader =
			events->acquire(get_eer_asset_path(rle6.name));
		const auto grid = reader->get_descriptor().get_extents();

		REQUIRE( extents(grid.begin(), grid.end()) == extents{32, 32} );

		detector_event_timeline timeline(2);
		reader->read(0, 3, timeline);

		REQUIRE( timeline.get_group_count() == 2 );
		REQUIRE( timeline.get_timestamp(0) == 0 );
		REQUIRE( timeline.get_timestamp(1) == 2 );
		REQUIRE( timeline.get_event_count() == 52 );
	}
}

TEST_CASE(
	"EER files are read as movies of the fractions they are cut into",
	"[eer][detector_event_image_reader_provider]" )
{
	SECTION( "a fraction per frame, on every subpixel" )
	{
		const auto frames = expected_frames(rle7);
		const auto movie = read_movie(rle7, 4, {1, 1});

		REQUIRE( extents_of(movie) == extents{4, 24, 32} );
		REQUIRE( movie.get_descriptor().get_data_type() ==
			numerical_type::uint32 );
		for (std::size_t frame = 0; frame < 4; ++frame)
		{
			REQUIRE( values_of(movie, frame * 768, 768) == frames[frame] );
		}
	}

	SECTION( "fewer fractions than frames, some of them longer" )
	{
		// Three fractions of four frames begin at frames 0, 1 and 2.
		const auto frames = expected_frames(rle7);
		const auto movie = read_movie(rle7, 3, {1, 1});

		REQUIRE( extents_of(movie) == extents{3, 24, 32} );
		REQUIRE( values_of(movie, 0, 768) == add_frames(frames, 0, 1) );
		REQUIRE( values_of(movie, 768, 768) == add_frames(frames, 1, 2) );
		REQUIRE( values_of(movie, 1536, 768) == add_frames(frames, 2, 4) );
	}

	SECTION( "a single fraction at the resolution of the detector" )
	{
		const auto frames = expected_frames(rle7);
		const auto sum = add_frames(frames, 0, 4);
		const auto image = read_movie(rle7, 1, {4, 4});

		REQUIRE( extents_of(image) == extents{6, 8} );

		counts binned(48, 0);
		for (std::size_t row = 0; row < 24; ++row)
		{
			for (std::size_t column = 0; column < 32; ++column)
			{
				binned[(row / 4) * 8 + column / 4] += sum[row * 32 + column];
			}
		}
		REQUIRE( values_of(image, 0, 48) == binned );
	}

	SECTION( "an empty frame is an empty fraction" )
	{
		const auto frames = expected_frames(rle6);
		const auto movie = read_movie(rle6, 3, {1, 1});

		REQUIRE( extents_of(movie) == extents{3, 32, 32} );
		REQUIRE( values_of(movie, 0, 1024) == frames[0] );
		REQUIRE( values_of(movie, 1024, 1024) == counts(1024, 0) );
		REQUIRE( values_of(movie, 2048, 1024) == frames[2] );
	}
}

TEST_CASE(
	"an EER file is not read as a TIFF image",
	"[eer][image_file_read_format_selector]" )
{
	file_image_reader_provider readers(
		image_file_read_format_selector::get_shared());

	REQUIRE_THROWS_AS(
		readers.acquire(get_eer_asset_path(rle7.name)),
		unsupported_operation_error
	);
}
