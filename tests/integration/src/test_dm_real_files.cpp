// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_file_read_format.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/tests/assets.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/transfers.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

// The values tests/assets/dm/README.txt states each file holds, element p
// being the p-th in the order they are stored.

std::vector<float> float32_values(std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t p = 0; p < count; ++p)
	{
		values[p] = static_cast<float>(p) * 0.25f - 4.0f;
	}

	return values;
}

std::vector<std::uint16_t> uint16_values(std::size_t count)
{
	std::vector<std::uint16_t> values(count);
	for (std::size_t p = 0; p < count; ++p)
	{
		values[p] = static_cast<std::uint16_t>(1000 + 3 * p);
	}

	return values;
}

std::vector<std::int16_t> stack_values(std::size_t count)
{
	std::vector<std::int16_t> values(count);
	for (std::size_t p = 0; p < count; ++p)
	{
		values[p] = static_cast<std::int16_t>(100 * (p / 24) - p % 24);
	}

	return values;
}

image_transfer_plan whole_of(const std::vector<std::size_t> &extents)
{
	image_transfer_plan regions(
		image_transfer_shape(extents, extents.size(), extents.size())
	);
	const std::vector<std::size_t> origin(extents.size(), 0);
	regions.add(make_span(origin), make_span(origin));

	return regions;
}

template <typename T>
std::vector<T> read_whole(
	const image_reader &reader,
	numerical_type data_type
)
{
	const auto extents = reader.get_descriptor().get_extents();
	const std::vector<std::size_t> shape(extents.begin(), extents.end());

	auto destination = make_host_array<T>(shape, data_type, T());
	read_now(reader, destination, whole_of(shape));

	return get_values<T>(destination);
}

image_descriptor make_descriptor(
	std::vector<std::size_t> extents,
	numerical_type data_type
)
{
	return image_descriptor(make_span(extents), 2, data_type);
}

} // anonymous namespace

TEST_CASE(
	"DigitalMicrograph files this project did not write are claimed",
	"[dm][image_file_format_selector]"
)
{
	image_file_read_format_selector selector;
	selector.register_builtin_formats();

	const std::vector<std::string> names = {
		"image_float32.dm3",
		"image_uint16_unaligned.dm4",
		"stack_int16.dm4",
		"image_float32_big_endian.dm3"
	};
	for (const auto &name : names)
	{
		const auto *format = selector.get_most_suitable_format(
			image_file_probe(get_dm_asset_path(name))
		);

		REQUIRE( format != nullptr );
		REQUIRE( format->get_name() == "DM" );
	}
}

TEST_CASE(
	"DigitalMicrograph files this project did not write are read as they "
	"state",
	"[dm][image_file_format_selector]"
)
{
	image_file_read_format_selector selector;
	selector.register_builtin_formats();

	SECTION( "an image of version 3 is read past its thumbnail" )
	{
		const auto reader =
			selector.open(get_dm_asset_path("image_float32.dm3"));

		REQUIRE( reader->get_descriptor() ==
			make_descriptor({6, 8}, numerical_type::float32) );
		REQUIRE( read_whole<float>(*reader, numerical_type::float32) ==
			float32_values(48) );
	}

	SECTION( "an image of version 4 whose values are unaligned is read" )
	{
		const auto reader =
			selector.open(get_dm_asset_path("image_uint16_unaligned.dm4"));

		REQUIRE( reader->get_descriptor() ==
			make_descriptor({5, 7}, numerical_type::uint16) );
		REQUIRE( read_whole<std::uint16_t>(*reader, numerical_type::uint16) ==
			uint16_values(35) );
	}

	SECTION( "a stack is read with its last extent as its first axis" )
	{
		const auto reader =
			selector.open(get_dm_asset_path("stack_int16.dm4"));

		REQUIRE( reader->get_descriptor() ==
			make_descriptor({3, 4, 6}, numerical_type::int16) );
		REQUIRE( read_whole<std::int16_t>(*reader, numerical_type::int16) ==
			stack_values(72) );
	}

	SECTION( "big-endian values are read in the byte order of the host" )
	{
		const auto reader = selector.open(
			get_dm_asset_path("image_float32_big_endian.dm3")
		);

		REQUIRE( reader->get_descriptor() ==
			make_descriptor({4, 5}, numerical_type::float32) );
		REQUIRE( read_whole<float>(*reader, numerical_type::float32) ==
			float32_values(20) );
	}
}
