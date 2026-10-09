// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/tiff/tiff_file_read_format.hpp>

#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_reader.hpp>

#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/whole_region_plan.hpp>
#include "fixtures/tiff_test_file.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::tiff;
using namespace vitrio::test;

namespace
{

void write_image(const std::string &path, const char *mode)
{
	write_striped_file<std::uint8_t>(
		path, mode, 4, 3, SAMPLEFORMAT_UINT, COMPRESSION_LZW, 3,
		{counting<std::uint8_t>(12)}
	);
}

} // anonymous namespace

TEST_CASE( "the TIFF format claims the files it can read",
	"[tiff_file_read_format]" )
{
	const scoped_path path("tiff_file_read_format_claimed.tif");
	const tiff_file_read_format format;

	SECTION( "it is named" )
	{
		REQUIRE( format.get_name() == "TIFF" );
	}

	SECTION( "a classic file of either byte order is claimed" )
	{
		write_image(path.get(), "wl");

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::normal );

		write_image(path.get(), "wb");

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::normal );
	}

	SECTION( "a BigTIFF file is claimed" )
	{
		write_image(path.get(), "w8");

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::normal );
	}

	SECTION( "the signature is claimed whatever the extension" )
	{
		const std::string other = path.get() + ".unknown";
		write_image(other, "w");

		REQUIRE( format.get_suitability(image_file_probe(other)) ==
			image_file_format_suitability::normal );

		std::remove(other.c_str());
	}

	SECTION( "a file of the extension that holds something else is not" )
	{
		write_file(path.get(), std::vector<char>(64, 'x'));

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::unsupported );
	}

	SECTION( "a file that is not there is not claimed" )
	{
		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::unsupported );
	}
}

TEST_CASE( "the TIFF read format opens a file as its pages state it",
	"[tiff_file_read_format]" )
{
	const scoped_path path("tiff_file_read_format_opened.tif");
	const tiff_file_read_format format;

	SECTION( "a claimed file opens into a reader of its values" )
	{
		write_image(path.get(), "w");

		const auto reader = format.open(image_file_probe(path.get()));

		const std::vector<std::size_t> extents = {3, 4};

		REQUIRE( reader != nullptr );
		REQUIRE( reader->get_descriptor() == image_descriptor(
			make_span(extents), 2, numerical_type::uint8) );

		auto destination =
			make_host_array<std::uint8_t>(extents, numerical_type::uint8);
		reader->read(array_ref(destination), whole_of(extents));

		REQUIRE( get_values<std::uint8_t>(destination) ==
			counting<std::uint8_t>(12) );
	}

	SECTION( "a file of several pages opens as a stack" )
	{
		write_striped_file<std::int16_t>(
			path.get(), "w", 4, 3, SAMPLEFORMAT_INT, COMPRESSION_NONE, 3,
			{counting<std::int16_t>(12), counting<std::int16_t>(12)}
		);

		const auto reader = format.open(image_file_probe(path.get()));

		const std::vector<std::size_t> extents = {2, 3, 4};

		REQUIRE( reader->get_descriptor() == image_descriptor(
			make_span(extents), 2, numerical_type::int16) );
	}

	SECTION( "a file this format can not transfer does not open" )
	{
		write_rgb_file(path.get(), 4, 3);

		const image_file_probe probe(path.get());

		REQUIRE( format.get_suitability(probe) ==
			image_file_format_suitability::normal );
		REQUIRE_THROWS_AS( format.open(probe), image_file_format_error );
	}
}
