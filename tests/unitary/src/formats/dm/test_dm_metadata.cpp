// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <formats/dm/dm_metadata.hpp>

#include <formats/dm/dm_constants.hpp>
#include <formats/dm/dm_geometry.hpp>
#include <formats/dm/dm_header.hpp>
#include <formats/dm/dm_tag_parser.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/image_metadata_fields.hpp>
#include <vitrio/span.hpp>

#include "fixtures/dm_test_file.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::dm;
using namespace vitrio::test;
using namespace vitrio::image_metadata_fields;

namespace
{

// What the image of a file states, the file being its bytes.
image_metadata derive(const std::vector<char> &raw)
{
	const auto bytes =
		make_span(reinterpret_cast<const byte*>(raw.data()), raw.size());
	const auto header = parse_header(bytes);
	const auto root = parse_tags(bytes, header);
	const auto order = header.get_byte_order();

	return derive_metadata(find_image(root, bytes, order), bytes, order);
}

image_metadata derive(const std::vector<dm_test_calibration> &axes)
{
	return derive(make_calibrated_file(4, true, axes));
}

// The pixel size along x of an image whose columns are a step of two of a
// unit apart, or zero if it states none.
double pixel_size_in(const std::u16string &units)
{
	const auto metadata = derive({{2.0F, units}, {2.0F, units}});
	const auto *found = find_field(metadata, pixel_size_x);
	return found ? *found : 0.0;
}

// A file of one image whose ImageData holds what a case puts in it.
dm_test_file begin_image(std::uint64_t image_data_count)
{
	dm_test_file file(4, true, 1);
	file.group("ImageList", 1).group("", 1)
		.group("ImageData", image_data_count);
	return file;
}

} // anonymous namespace

TEST_CASE(
	"the pixel sizes of a DigitalMicrograph image are the scales of its "
	"axes",
	"[dm_metadata]"
)
{
	SECTION( "along its columns and its rows, in that order" )
	{
		const auto metadata = derive({{0.25F, u"nm"}, {0.5F, u"nm"}});

		REQUIRE( get_field(metadata, pixel_size_x) == 2.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 5.0 );
		REQUIRE( metadata.get_entries().size() == 2 );
	}

	SECTION( "in either version of the format and either byte order" )
	{
		const std::uint32_t version = GENERATE(3U, 4U);
		const bool little_endian = GENERATE(true, false);

		const auto metadata = derive(make_calibrated_file(
			version, little_endian, {{0.25F, u"nm"}, {0.5F, u"nm"}}));

		REQUIRE( get_field(metadata, pixel_size_x) == 2.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 5.0 );
	}

	// The third axis of a stack is the one it stacks its images on,
	// whatever it is calibrated in.
	SECTION( "and a stack of images has none along z" )
	{
		const auto metadata =
			derive({{0.25F, u"nm"}, {0.5F, u"nm"}, {4.0F, u"nm"}});

		REQUIRE( get_field(metadata, pixel_size_x) == 2.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 5.0 );
		REQUIRE( find_field(metadata, pixel_size_z) == nullptr );
	}

	SECTION( "along the one axis that is calibrated" )
	{
		const auto metadata = derive({{1.0F, u""}, {0.5F, u"nm"}});

		REQUIRE( find_field(metadata, pixel_size_x) == nullptr );
		REQUIRE( get_field(metadata, pixel_size_y) == 5.0 );
	}

	SECTION( "a scale of either width of real" )
	{
		auto file = begin_image(1);
		file.group("Calibrations", 1).group("Dimension", 1).group("", 2)
			.scalar<double>("Scale", encoding::float64, 0.25)
			.array<std::uint16_t>("Units", encoding::uint16, {'n', 'm'});

		REQUIRE( get_field(derive(file.finish()), pixel_size_x) == 2.5 );
	}
}

TEST_CASE(
	"a length of DigitalMicrograph is so many ångströms",
	"[dm_metadata]"
)
{
	REQUIRE( pixel_size_in(u"nm") == 20.0 );
	REQUIRE( pixel_size_in(u"pm") == 0.02 );
	REQUIRE( pixel_size_in(u"mm") == 2.0e7 );

	// The micro sign and the letter mu, and the letter the files of those
	// who have neither use.
	REQUIRE( pixel_size_in(u"µm") == 2.0e4 );
	REQUIRE( pixel_size_in(u"μm") == 2.0e4 );
	REQUIRE( pixel_size_in(u"um") == 2.0e4 );

	// The letter and the sign.
	REQUIRE( pixel_size_in(u"Å") == 2.0 );
	REQUIRE( pixel_size_in(u"Å") == 2.0 );

	// What is calibrated in no length: an image that is not calibrated, a
	// diffraction pattern, a spectrum, a series in time.
	REQUIRE( pixel_size_in(u"") == 0.0 );
	REQUIRE( pixel_size_in(u"1/nm") == 0.0 );
	REQUIRE( pixel_size_in(u"eV") == 0.0 );
	REQUIRE( pixel_size_in(u"s") == 0.0 );
	REQUIRE( pixel_size_in(u"NM") == 0.0 );
	REQUIRE( pixel_size_in(u"nm ") == 0.0 );
}

TEST_CASE(
	"a DigitalMicrograph image states no pixel size where it has none",
	"[dm_metadata]"
)
{
	SECTION( "a scale that is no distance" )
	{
		const auto metadata = derive({
			{0.0F, u"nm"},
			{-0.5F, u"nm"}
		});

		REQUIRE( metadata.get_entries().empty() );
		REQUIRE(
			derive({{std::numeric_limits<float>::quiet_NaN(), u"nm"}})
				.get_entries().empty()
		);
		REQUIRE(
			derive({{std::numeric_limits<float>::infinity(), u"nm"}})
				.get_entries().empty()
		);
	}

	SECTION( "an image with no calibrations" )
	{
		auto file = begin_image(1);
		file.scalar<std::int32_t>("DataType", encoding::int32, 2);

		REQUIRE( derive(file.finish()).get_entries().empty() );
	}

	SECTION( "an image with no ImageData" )
	{
		dm_test_file file(4, true, 1);
		file.group("ImageList", 1).group("", 0);

		REQUIRE( derive(file.finish()).get_entries().empty() );
	}

	SECTION( "calibrations of no axis" )
	{
		REQUIRE( derive(std::vector<dm_test_calibration>()).get_entries()
			.empty() );
	}

	SECTION( "an axis with no scale, or with no units" )
	{
		auto file = begin_image(1);
		file.group("Calibrations", 1).group("Dimension", 2)
			.group("", 1)
			.array<std::uint16_t>("Units", encoding::uint16, {'n', 'm'})
			.group("", 1)
			.scalar<float>("Scale", encoding::float32, 0.5F);

		REQUIRE( derive(file.finish()).get_entries().empty() );
	}

	SECTION( "a scale that is no real, which is not taken for one" )
	{
		auto file = begin_image(1);
		file.group("Calibrations", 1).group("Dimension", 1).group("", 2)
			.scalar<std::int32_t>("Scale", encoding::int32, 2)
			.array<std::uint16_t>("Units", encoding::uint16, {'n', 'm'});

		REQUIRE( derive(file.finish()).get_entries().empty() );
	}

	SECTION( "units that are no text" )
	{
		auto file = begin_image(1);
		file.group("Calibrations", 1).group("Dimension", 1).group("", 2)
			.scalar<float>("Scale", encoding::float32, 0.5F)
			.array<std::uint8_t>("Units", encoding::uint8, {'n', 'm'});

		REQUIRE( derive(file.finish()).get_entries().empty() );
	}

	SECTION( "an axis that is a datum and no group" )
	{
		auto file = begin_image(1);
		file.group("Calibrations", 1).group("Dimension", 1)
			.scalar<float>("", encoding::float32, 0.5F);

		REQUIRE( derive(file.finish()).get_entries().empty() );
	}
}
