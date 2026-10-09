// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_format_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_probe.hpp>
#include <vitrio/image_read_format_selector.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/tests/assets.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

// The samples of each asset, restated from the formulas its README gives
// rather than read off the file by other means.
std::vector<std::uint8_t> uint8_samples(std::size_t count)
{
	std::vector<std::uint8_t> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<std::uint8_t>((7 * i + 3) % 251);
	}

	return values;
}

std::vector<std::uint16_t> uint16_samples(std::size_t count)
{
	std::vector<std::uint16_t> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<std::uint16_t>(1000 + 257 * i);
	}

	return values;
}

std::vector<std::int16_t> int16_samples(std::size_t count)
{
	std::vector<std::int16_t> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<std::int16_t>(37 * static_cast<int>(i) - 300);
	}

	return values;
}

std::vector<float> float32_samples(std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = 0.25F * static_cast<float>(i) - 3.5F;
	}

	return values;
}

} // anonymous namespace

TEST_CASE(
	"TIFF files vitrio did not write are read as they state",
	"[tiff][image_format_selector]"
)
{
	image_read_format_selector selector;
	selector.register_builtin_formats();

	SECTION( "a compressed stack cut into several strips" )
	{
		const auto path = get_tiff_asset_path("stack_uint8_lzw.tif");
		const std::vector<std::size_t> extents = {3, 6, 8};

		REQUIRE( selector.get_most_suitable_format(image_probe(path)) !=
			nullptr );

		const auto reader = selector.open(path);

		REQUIRE( reader->get_descriptor() ==
			image_descriptor(make_span(extents), 2, numerical_type::uint8) );

		auto destination =
			make_host_array<std::uint8_t>(extents, numerical_type::uint8);
		reader->read(array_ref(destination), whole_of(extents));

		REQUIRE( get_values<std::uint8_t>(destination) ==
			uint8_samples(144) );
	}

	SECTION( "an image compressed with Deflate after a predictor" )
	{
		const auto path =
			get_tiff_asset_path("image_uint16_deflate_predictor.tif");
		const std::vector<std::size_t> extents = {6, 8};

		const auto reader = selector.open(path);

		REQUIRE( reader->get_descriptor() ==
			image_descriptor(make_span(extents), 2, numerical_type::uint16) );

		auto destination =
			make_host_array<std::uint16_t>(extents, numerical_type::uint16);
		reader->read(array_ref(destination), whole_of(extents));

		REQUIRE( get_values<std::uint16_t>(destination) ==
			uint16_samples(48) );
	}

	SECTION( "an image cut into tiles that reach past it" )
	{
		const auto path = get_tiff_asset_path("image_float32_tiled.tif");
		const std::vector<std::size_t> extents = {24, 40};

		const auto reader = selector.open(path);

		REQUIRE( reader->get_descriptor() ==
			image_descriptor(make_span(extents), 2, numerical_type::float32) );

		auto destination =
			make_host_array<float>(extents, numerical_type::float32);
		reader->read(array_ref(destination), whole_of(extents));

		REQUIRE( get_values<float>(destination) ==
			float32_samples(960) );
	}

	SECTION( "an image in the other byte order" )
	{
		const auto path = get_tiff_asset_path("image_int16_big_endian.tif");
		const std::vector<std::size_t> extents = {6, 8};

		const auto reader = selector.open(path);

		REQUIRE( reader->get_descriptor() ==
			image_descriptor(make_span(extents), 2, numerical_type::int16) );

		auto destination =
			make_host_array<std::int16_t>(extents, numerical_type::int16);
		reader->read(array_ref(destination), whole_of(extents));

		REQUIRE( get_values<std::int16_t>(destination) ==
			int16_samples(48) );
	}

	SECTION( "a BigTIFF stack" )
	{
		const auto path = get_tiff_asset_path("stack_uint8_bigtiff.tif");
		const std::vector<std::size_t> extents = {2, 6, 8};

		const auto reader = selector.open(path);

		REQUIRE( reader->get_descriptor() ==
			image_descriptor(make_span(extents), 2, numerical_type::uint8) );

		auto destination =
			make_host_array<std::uint8_t>(extents, numerical_type::uint8);
		reader->read(array_ref(destination), whole_of(extents));

		REQUIRE( get_values<std::uint8_t>(destination) ==
			uint8_samples(96) );
	}
}

TEST_CASE(
	"regions of a real TIFF file arrive where they are placed",
	"[tiff][image_format_selector]"
)
{
	image_read_format_selector selector;
	selector.register_builtin_formats();

	SECTION( "one page of a stack, converted on the way" )
	{
		const auto reader =
			selector.open(get_tiff_asset_path("stack_uint8_lzw.tif"));

		const std::vector<std::size_t> extents = {6, 8};
		auto destination =
			make_host_array<float>(extents, numerical_type::float32);
		image_transfer_plan regions(image_transfer_shape(extents, 3, 2));
		regions.add(
			make_span(std::vector<std::size_t>{2, 0, 0}),
			make_span(std::vector<std::size_t>{0, 0})
		);

		reader->read(array_ref(destination), regions);

		const auto samples = uint8_samples(144);

		REQUIRE( get_values<float>(destination) ==
			std::vector<float>(samples.begin() + 96, samples.end()) );
	}

	SECTION( "a patch across the tiles of an image" )
	{
		const auto reader =
			selector.open(get_tiff_asset_path("image_float32_tiled.tif"));

		// Two rows from the fifteenth and three columns from the thirty
		// first, where two rows and two columns of tiles meet.
		const std::vector<std::size_t> extents = {2, 3};
		auto destination =
			make_host_array<float>(extents, numerical_type::float32);
		image_transfer_plan regions(image_transfer_shape(extents, 2, 2));
		regions.add(
			make_span(std::vector<std::size_t>{15, 31}),
			make_span(std::vector<std::size_t>{0, 0})
		);

		reader->read(array_ref(destination), regions);

		const auto samples = float32_samples(960);

		REQUIRE( get_values<float>(destination) ==
			std::vector<float>{
				samples[15 * 40 + 31],
				samples[15 * 40 + 32],
				samples[15 * 40 + 33],
				samples[16 * 40 + 31],
				samples[16 * 40 + 32],
				samples[16 * 40 + 33]
			} );
	}
}

TEST_CASE(
	"well formed TIFF files vitrio does not support are refused",
	"[tiff][image_format_selector]"
)
{
	image_read_format_selector selector;
	selector.register_builtin_formats();

	const std::vector<std::string> names = {
		"refused_rgb.tif",
		"refused_pages_differ.tif"
	};
	for (const auto &name : names)
	{
		const auto path = get_tiff_asset_path(name);

		REQUIRE( selector.get_most_suitable_format(image_probe(path)) !=
			nullptr );
		REQUIRE_THROWS_AS( selector.open(path), image_format_error );
	}
}
