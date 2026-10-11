// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <formats/dm/dm_file_read_format.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <vitrio/tests/scoped_path.hpp>
#include "fixtures/dm_test_file.hpp"

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::dm;
using namespace vitrio::test;

namespace
{

// Write a file of a counting float32 image, its values aligned or not as
// asked.
void write_image_file(
	const std::string &path,
	std::uint32_t version,
	bool little_endian,
	const std::vector<std::uint32_t> &dimensions,
	bool aligned
)
{
	for (std::size_t padding = 0; padding < sizeof(float); ++padding)
	{
		std::size_t data_offset = 0;
		const auto raw = make_image_file(
			version,
			little_endian,
			make_counting_image(little_endian, dimensions),
			padding,
			data_offset
		);
		if ((data_offset % sizeof(float) == 0) == aligned)
		{
			write_file(path, raw);
			return;
		}
	}
}

std::vector<float> read_whole(const image_reader &reader)
{
	const auto extents = reader.get_descriptor().get_extents();
	const std::vector<std::size_t> shape(extents.begin(), extents.end());

	auto destination = make_array(
		make_contiguous_array_descriptor(
			make_span(shape),
			numerical_type::float32
		)
	);

	image_transfer_plan regions(
		image_transfer_shape(shape, shape.size(), shape.size())
	);
	const std::vector<std::size_t> origin(shape.size(), 0);
	regions.add(make_span(origin), make_span(origin));

	reader.read(array_ref(destination), regions);

	std::size_t count = 1;
	for (const auto extent : shape)
	{
		count *= extent;
	}

	const auto *data = reinterpret_cast<const float*>(destination.get_data());
	return std::vector<float>(data, data + count);
}

} // anonymous namespace

TEST_CASE( "the DM format claims the files it can read",
	"[dm_file_read_format]" )
{
	const scoped_path path("dm_reader_claimed.dm4");
	const dm_file_read_format format;

	SECTION( "it is named" )
	{
		REQUIRE( format.get_name() == "DM" );
	}

	SECTION( "a file that begins as one of its own and is named so is "
		"claimed" )
	{
		write_image_file(path.get(), 4, true, {4, 3}, true);

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::normal );
	}

	SECTION( "one that only begins as one of its own is claimed weakly" )
	{
		const std::string other = path.get() + ".unknown";
		write_image_file(other, 3, true, {4, 3}, true);

		REQUIRE( format.get_suitability(image_file_probe(other)) ==
			image_file_format_suitability::fallback );

		std::remove(other.c_str());
	}

	SECTION( "one that is only named so is not claimed" )
	{
		write_file(path.get(), std::vector<char>(64, '\0'));

		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::unsupported );
	}

	SECTION( "a file that is not there is not claimed" )
	{
		REQUIRE( format.get_suitability(image_file_probe(path.get())) ==
			image_file_format_suitability::unsupported );
	}
}

TEST_CASE( "the DM read format opens a file as its tags state it",
	"[dm_file_read_format]" )
{
	const scoped_path path("dm_read_format_opened.dm3");
	const dm_file_read_format format;

	const std::uint32_t version = GENERATE(3, 4);
	const bool little_endian = GENERATE(true, false);
	const bool aligned = GENERATE(true, false);

	write_image_file(path.get(), version, little_endian, {5, 4, 3}, aligned);

	const auto reader = format.open(image_file_probe(path.get()));

	const std::vector<std::size_t> extents = {3, 4, 5};
	REQUIRE( reader->get_descriptor() ==
		image_descriptor(make_span(extents), 2, numerical_type::float32) );
	REQUIRE( read_whole(*reader) == counting(60) );
}

TEST_CASE( "the DM read format refuses what it can not open",
	"[dm_file_read_format]" )
{
	const scoped_path path("dm_read_format_refused.dm4");
	const dm_file_read_format format;

	SECTION( "a file whose tags are cut short" )
	{
		std::size_t data_offset = 0;
		auto raw = make_image_file(
			4, true, make_counting_image(true, {4, 4}), 0, data_offset
		);
		raw.resize(data_offset + 8);
		write_file(path.get(), raw);

		REQUIRE_THROWS_AS(
			format.open(image_file_probe(path.get())),
			image_file_format_error
		);
	}

	SECTION( "a file of an image of RGB values" )
	{
		std::size_t data_offset = 0;
		auto image = make_counting_image(true, {4, 4});
		image.data_type = 8;
		write_file(
			path.get(),
			make_image_file(4, true, image, 0, data_offset)
		);

		REQUIRE_THROWS_AS(
			format.open(image_file_probe(path.get())),
			unsupported_operation_error
		);
	}
}
