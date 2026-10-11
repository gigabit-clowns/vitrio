// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <formats/dm/dm_constants.hpp>
#include <formats/dm/dm_geometry.hpp>
#include <formats/dm/dm_header.hpp>
#include <formats/dm/dm_tag_parser.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/span.hpp>

#include "fixtures/dm_test_file.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace vitrio;
using namespace vitrio::dm;
using namespace vitrio::test;

namespace
{

dm_image_location locate(const std::vector<char> &raw)
{
	const auto bytes =
		make_span(reinterpret_cast<const byte*>(raw.data()), raw.size());
	const auto header = parse_header(bytes);

	return locate_image(
		parse_tags(bytes, header),
		bytes,
		header.get_byte_order()
	);
}

image_descriptor make_descriptor(
	std::vector<std::size_t> extents,
	numerical_type data_type
)
{
	return image_descriptor(make_span(extents), 2, data_type);
}

// A file whose image list holds the given images, and whose Thumbnails
// group names the given positions of it.
std::vector<char> make_listed_file(
	const std::vector<dm_test_image> &images,
	const std::vector<std::uint32_t> &thumbnails
)
{
	dm_test_file file(4, true, 2);
	file.group("ImageList", images.size());
	for (const auto &image : images)
	{
		put_image(file, image);
	}

	file.group("Thumbnails", thumbnails.size());
	for (const auto index : thumbnails)
	{
		file.group("", 1)
			.scalar<std::uint32_t>("ImageIndex", encoding::uint32, index);
	}

	return file.finish();
}

dm_test_image make_byte_image(std::uint32_t width)
{
	return dm_test_image{
		6,
		encoding::uint8,
		{width, 1},
		std::vector<char>(width, '\0'),
		1
	};
}

} // anonymous namespace

TEST_CASE( "the image of a DigitalMicrograph file is found past its "
	"thumbnail", "[dm_geometry]" )
{
	const std::uint32_t version = GENERATE(3, 4);
	const bool little_endian = GENERATE(true, false);

	std::size_t data_offset = 0;
	const auto raw = make_image_file(
		version,
		little_endian,
		make_counting_image(little_endian, {5, 4}),
		3,
		data_offset
	);

	const auto location = locate(raw);

	REQUIRE( location.get_descriptor() ==
		make_descriptor({4, 5}, numerical_type::float32) );
	REQUIRE( location.get_data_offset() == data_offset );
	REQUIRE( location.get_byte_order() == (little_endian
		? byte_order::little_endian
		: byte_order::big_endian) );
}

TEST_CASE( "the image of a file is the first that is not a thumbnail",
	"[dm_geometry]" )
{
	SECTION( "when no thumbnail is named, the first image" )
	{
		const auto location = locate(make_listed_file(
			{make_byte_image(3), make_byte_image(4)},
			{}
		));

		REQUIRE( location.get_descriptor() ==
			make_descriptor({1, 3}, numerical_type::uint8) );
	}

	SECTION( "past every image the group names" )
	{
		const auto location = locate(make_listed_file(
			{make_byte_image(3), make_byte_image(4), make_byte_image(5)},
			{1, 0}
		));

		REQUIRE( location.get_descriptor() ==
			make_descriptor({1, 5}, numerical_type::uint8) );
	}

	SECTION( "a file of nothing but thumbnails is refused" )
	{
		REQUIRE_THROWS_AS(
			locate(make_listed_file({make_byte_image(3)}, {0})),
			image_file_format_error
		);
	}

	SECTION( "a file with no image list is refused" )
	{
		dm_test_file file(3, true, 1);
		file.scalar<std::int32_t>("Other", encoding::int32, 0);

		REQUIRE_THROWS_AS( locate(file.finish()), image_file_format_error );
	}
}

TEST_CASE( "a stack takes its last extent as its first axis",
	"[dm_geometry]" )
{
	std::size_t data_offset = 0;
	const auto raw = make_image_file(
		4,
		true,
		make_counting_image(true, {6, 4, 3}),
		0,
		data_offset
	);

	REQUIRE( locate(raw).get_descriptor() ==
		make_descriptor({3, 4, 6}, numerical_type::float32) );
}

TEST_CASE( "an image that is not read as it states is refused",
	"[dm_geometry]" )
{
	std::size_t data_offset = 0;

	SECTION( "one of a single axis" )
	{
		REQUIRE_THROWS_AS(
			locate(make_image_file(
				3, true, make_counting_image(true, {8}), 0, data_offset)),
			unsupported_operation_error
		);
	}

	SECTION( "one of four axes" )
	{
		REQUIRE_THROWS_AS(
			locate(make_image_file(
				3, true, make_counting_image(true, {2, 2, 2, 2}), 0,
				data_offset)),
			unsupported_operation_error
		);
	}

	SECTION( "one of RGB values" )
	{
		auto image = make_counting_image(true, {2, 2});
		image.data_type = 23;

		REQUIRE_THROWS_AS(
			locate(make_image_file(3, true, image, 0, data_offset)),
			unsupported_operation_error
		);
	}

	SECTION( "one whose values are fewer than its extents make" )
	{
		auto image = make_counting_image(true, {4, 4});
		image.values.resize(image.values.size() - sizeof(float));

		REQUIRE_THROWS_AS(
			locate(make_image_file(3, true, image, 0, data_offset)),
			image_file_format_error
		);
	}

	SECTION( "one whose extents would overflow" )
	{
		auto image = make_counting_image(true, {4, 4});
		image.dimensions = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

		REQUIRE_THROWS_AS(
			locate(make_image_file(3, true, image, 0, data_offset)),
			image_file_format_error
		);
	}

	SECTION( "one that states a negative extent" )
	{
		dm_test_file file(3, true, 1);
		file.group("ImageList", 1)
			.group("", 1)
			.group("ImageData", 3)
			.scalar<std::int32_t>("DataType", encoding::int32, 2)
			.group("Dimensions", 2)
			.scalar<std::int32_t>("", encoding::int32, -4)
			.scalar<std::int32_t>("", encoding::int32, 1)
			.array<float>("Data", encoding::float32, {});

		REQUIRE_THROWS_AS( locate(file.finish()), image_file_format_error );
	}

	SECTION( "one with no data type" )
	{
		dm_test_file file(3, true, 1);
		file.group("ImageList", 1)
			.group("", 1)
			.group("ImageData", 2)
			.group("Dimensions", 2)
			.scalar<std::int32_t>("", encoding::int32, 1)
			.scalar<std::int32_t>("", encoding::int32, 1)
			.array<float>("Data", encoding::float32, {0.0f});

		REQUIRE_THROWS_AS( locate(file.finish()), image_file_format_error );
	}

	SECTION( "one whose values are a group" )
	{
		dm_test_file file(3, true, 1);
		file.group("ImageList", 1)
			.group("", 1)
			.group("ImageData", 3)
			.scalar<std::int32_t>("DataType", encoding::int32, 2)
			.group("Dimensions", 2)
			.scalar<std::int32_t>("", encoding::int32, 1)
			.scalar<std::int32_t>("", encoding::int32, 1)
			.group("Data", 0);

		REQUIRE_THROWS_AS( locate(file.finish()), image_file_format_error );
	}
}
