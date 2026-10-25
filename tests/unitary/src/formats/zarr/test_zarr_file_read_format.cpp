// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/zarr/zarr_file_read_format.hpp>

#include <array/fixed_width_float.hpp>

#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>

#include <vitrio/tests/assets.hpp>
#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/scoped_path.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include "zarr_test_store.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <memory>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;
using namespace vitrio::zarr;

namespace
{

using extents = std::vector<std::size_t>;

std::shared_ptr<image_reader> open_asset(const std::string &name)
{
	const zarr_file_read_format format;
	const image_file_probe probe(get_zarr_asset_path(name));
	REQUIRE( format.get_suitability(probe) ==
		image_file_format_suitability::normal );
	return format.open(probe, *make_file_transfer_pool());
}

extents extents_of(const image_reader &reader)
{
	const auto values = reader.get_descriptor().get_extents();
	return extents(values.begin(), values.end());
}

template <typename T>
std::vector<T> read_whole(const image_reader &reader, numerical_type type)
{
	const auto shape = extents_of(reader);
	auto destination = make_host_array<T>(shape, type);
	read_now(reader, destination, whole_of(shape));
	return get_values<T>(destination);
}

// The values of README.txt: p, the position in C order, times a scale plus
// an offset.
template <typename T>
std::vector<T> counting(std::size_t count, double scale, double offset)
{
	std::vector<T> values(count);
	for (std::size_t p = 0; p < count; ++p)
	{
		values[p] = static_cast<T>(static_cast<double>(p) * scale + offset);
	}

	return values;
}

} // anonymous namespace

TEST_CASE( "zarr_file_read_format claims the directories of ZARR",
	"[zarr_file_read_format]" )
{
	const zarr_file_read_format format;

	SECTION( "it is named" )
	{
		REQUIRE( format.get_name() == "ZARR" );
	}

	SECTION( "an array or a group of either version, whatever its name" )
	{
		for (const auto *name : {
			"v2_float32_blosc_lz4.zarr", "v3_uint16_big_endian_gzip.zarr",
			"ome_v04_tomogram.zarr", "ome_v05_tilt_series.zarr/1",
		})
		{
			REQUIRE( format.get_suitability(image_file_probe(
				get_zarr_asset_path(name))) ==
				image_file_format_suitability::normal );
		}
	}

	SECTION( "a separator after the directory changes nothing" )
	{
		REQUIRE( format.get_suitability(image_file_probe(
			get_zarr_asset_path("v2_bool.zarr") + "/")) ==
			image_file_format_suitability::normal );
	}

	SECTION( "not a directory without its metadata, a file, or nothing" )
	{
		const scoped_directory empty("test_zarr_format_empty.zarr");
		boost::filesystem::create_directories(empty.get());
		const scoped_path file("test_zarr_format_file.zarr");
		std::ofstream(file.get().c_str()) << "{}";

		for (const auto &path : {
			empty.get(), file.get(),
			get_zarr_asset_path("v2_float32_blosc_lz4.zarr/0"),
			get_zarr_asset_path("absent.zarr"),
		})
		{
			REQUIRE( format.get_suitability(image_file_probe(path)) ==
				image_file_format_suitability::unsupported );
		}
	}
}

TEST_CASE( "zarr_file_read_format reads arrays of version 2",
	"[zarr_file_read_format]" )
{
	SECTION( "float32, blosc and lz4, chunks in directories" )
	{
		const auto reader = open_asset("v2_float32_blosc_lz4.zarr");
		REQUIRE( reader->get_descriptor() == image_descriptor(
			make_span(extents{5, 6, 7}), 3, numerical_type::float32) );
		REQUIRE( read_whole<float32_t>(*reader, numerical_type::float32) ==
			counting<float32_t>(210, 0.25, -4) );
	}

	SECTION( "int16 big endian in the order F, zlib" )
	{
		const auto reader = open_asset("v2_int16_big_endian_f_zlib.zarr");
		REQUIRE( reader->get_descriptor() == image_descriptor(
			make_span(extents{6, 10}), 2, numerical_type::int16) );
		REQUIRE( read_whole<std::int16_t>(*reader, numerical_type::int16) ==
			counting<std::int16_t>(60, -3, 500) );
	}

	SECTION( "a chunk that is not stored is the fill value" )
	{
		const auto reader = open_asset("v2_uint8_gzip_missing_chunk.zarr");
		auto expected = counting<std::uint8_t>(24, 1, 20);
		for (std::size_t row = 2; row < 4; ++row)
		{
			for (std::size_t column = 0; column < 3; ++column)
			{
				expected[row * 6 + column] = 7;
			}
		}
		REQUIRE( read_whole<std::uint8_t>(*reader, numerical_type::uint8) ==
			expected );
	}

	SECTION( "a fill value of NaN" )
	{
		const auto reader = open_asset("v2_float32_zstd_nan_fill.zarr");
		const auto values =
			read_whole<float32_t>(*reader, numerical_type::float32);
		const auto written = counting<float32_t>(8, 1, 1);
		for (std::size_t i = 0; i < 16; ++i)
		{
			if (i < 8)
			{
				REQUIRE( values[i] == written[i] );
			}
			else
			{
				REQUIRE( std::isnan(values[i]) );
			}
		}
	}

	SECTION( "booleans, uncompressed" )
	{
		const auto reader = open_asset("v2_bool.zarr");
		REQUIRE( reader->get_descriptor().get_data_type() ==
			numerical_type::boolean );
		const auto values =
			read_whole<std::uint8_t>(*reader, numerical_type::uint8);
		for (std::size_t p = 0; p < values.size(); ++p)
		{
			REQUIRE( values[p] == (p % 3 == 0 ? 1 : 0) );
		}
	}
}

TEST_CASE( "zarr_file_read_format reads arrays of version 3",
	"[zarr_file_read_format]" )
{
	SECTION( "transposed, zstd and a checksum" )
	{
		const auto reader = open_asset("v3_float32_transpose_zstd_crc32c.zarr");
		REQUIRE( extents_of(*reader) == extents{3, 4, 5} );
		REQUIRE( read_whole<float32_t>(*reader, numerical_type::float32) ==
			counting<float32_t>(60, 0.5, 1) );
	}

	SECTION( "uint16 big endian, gzip, converted as it is read" )
	{
		const auto reader = open_asset("v3_uint16_big_endian_gzip.zarr");
		REQUIRE( read_whole<float64_t>(*reader, numerical_type::float64) ==
			counting<float64_t>(63, 7, 1000) );
	}
}

TEST_CASE( "zarr_file_read_format reads sharded arrays",
	"[zarr_file_read_format]" )
{
	SECTION( "its index at the end and checked, chunks and a shard unstored" )
	{
		const auto reader = open_asset("v3_uint16_sharded.zarr");
		REQUIRE( reader->get_descriptor() == image_descriptor(
			make_span(extents{6, 10}), 2, numerical_type::uint16) );

		auto expected = counting<std::uint16_t>(60, 3, 5);
		for (std::size_t row = 0; row < 6; ++row)
		{
			for (std::size_t column = 0; column < 10; ++column)
			{
				if ((row < 2 && column < 2) || (row >= 4 && column >= 8))
				{
					expected[row * 10 + column] = 0;
				}
			}
		}
		REQUIRE( read_whole<std::uint16_t>(*reader, numerical_type::uint16) ==
			expected );
	}

	SECTION( "its index at the start, its chunks transposed and big endian" )
	{
		const auto reader =
			open_asset("v3_float32_sharded_index_start.zarr");
		REQUIRE( read_whole<float32_t>(*reader, numerical_type::float32) ==
			counting<float32_t>(24, 0.5, -2) );
	}

	SECTION( "a patch that reaches some chunks of some shards" )
	{
		const auto reader = open_asset("v3_uint16_sharded.zarr");
		image_transfer_plan regions(image_transfer_shape({3, 3}, 2, 2));
		regions.add(make_span(extents{1, 3}), make_span(extents{0, 0}));
		auto destination = make_host_array<std::uint16_t>(
			{3, 3}, numerical_type::uint16);
		read_now(*reader, destination, regions);

		std::vector<std::uint16_t> expected;
		for (std::size_t row = 1; row < 4; ++row)
		{
			for (std::size_t column = 3; column < 6; ++column)
			{
				expected.push_back(
					static_cast<std::uint16_t>(3 * (row * 10 + column) + 5));
			}
		}
		REQUIRE( get_values<std::uint16_t>(destination) == expected );
	}
}

TEST_CASE( "zarr_file_read_format reads images of OME-Zarr",
	"[zarr_file_read_format]" )
{
	SECTION( "a group opens its finest level, a volume" )
	{
		const auto reader = open_asset("ome_v04_tomogram.zarr");
		REQUIRE( reader->get_descriptor() == image_descriptor(
			make_span(extents{8, 8, 8}), 3, numerical_type::float32) );
		REQUIRE( read_whole<float32_t>(*reader, numerical_type::float32) ==
			counting<float32_t>(512, 1, 0) );
	}

	SECTION( "a tilt series is a stack of images" )
	{
		const auto reader = open_asset("ome_v05_tilt_series.zarr");
		REQUIRE( reader->get_descriptor() == image_descriptor(
			make_span(extents{5, 8, 8}), 2, numerical_type::uint16) );
	}

	SECTION( "a level named on its own takes the axes of its group" )
	{
		const auto stack = open_asset("ome_v05_tilt_series.zarr/1");
		REQUIRE( stack->get_descriptor() == image_descriptor(
			make_span(extents{5, 4, 4}), 2, numerical_type::uint16) );

		const auto volume = open_asset("ome_v04_tomogram.zarr/2");
		REQUIRE( volume->get_descriptor() == image_descriptor(
			make_span(extents{2, 2, 2}), 3, numerical_type::float32) );
		const std::vector<float32_t> corners = {
			0, 4, 32, 36, 256, 260, 288, 292,
		};
		REQUIRE( read_whole<float32_t>(*volume, numerical_type::float32) ==
			corners );
	}

	SECTION( "a group that is no image is refused, naming it" )
	{
		const scoped_directory group("test_zarr_format_plain_group.zarr");
		write_store_file(group.get(), ".zgroup", R"({"zarr_format": 2})");
		const zarr_file_read_format format;

		REQUIRE_THROWS_MATCHES(
			format.open(image_file_probe(group.get()),
				*make_file_transfer_pool()),
			unsupported_operation_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith(group.get() + ": "))
		);
	}
}

TEST_CASE( "zarr_file_read_format reads regions across chunks",
	"[zarr_file_read_format]" )
{
	const auto reader = open_asset("v2_float32_blosc_lz4.zarr");
	const auto all = counting<float32_t>(210, 0.25, -4);
	const auto at = [&all] (std::size_t z, std::size_t y, std::size_t x)
	{
		return static_cast<float64_t>(all[(z * 6 + y) * 7 + x]);
	};

	SECTION( "patches of a volume into a stack of them" )
	{
		// Two patches of 3 by 3 by 4, which cross the chunks of 2 by 4 by 3
		// along every axis, side by side in an array of float64.
		image_transfer_plan regions(image_transfer_shape({3, 3, 4}, 3, 4));
		regions.add(
			make_span(extents{1, 2, 1}), make_span(extents{0, 0, 0, 0}));
		regions.add(
			make_span(extents{2, 3, 3}), make_span(extents{1, 0, 0, 0}));

		auto destination = make_host_array<float64_t>(
			{2, 3, 3, 4}, numerical_type::float64, -1.0);
		read_now(*reader, destination, regions);
		const auto values = get_values<float64_t>(destination);

		for (std::size_t z = 0; z < 3; ++z)
		{
			for (std::size_t y = 0; y < 3; ++y)
			{
				for (std::size_t x = 0; x < 4; ++x)
				{
					const auto index = (z * 3 + y) * 4 + x;
					REQUIRE( values[index] == at(z + 1, y + 2, x + 1) );
					REQUIRE( values[36 + index] == at(z + 2, y + 3, x + 3) );
				}
			}
		}
	}

	SECTION( "a region out of the array is reported" )
	{
		image_transfer_plan regions(image_transfer_shape({3, 3, 4}, 3, 3));
		regions.add(make_span(extents{4, 0, 0}), make_span(extents{0, 0, 0}));
		auto destination = make_host_array<float32_t>(
			{3, 3, 4}, numerical_type::float32);

		REQUIRE_THROWS_AS(
			read_now(*reader, destination, regions), std::out_of_range);
	}

	SECTION( "an empty plan reads nothing" )
	{
		const image_transfer_plan regions(
			image_transfer_shape({3, 3, 4}, 3, 3));
		auto destination = make_host_array<float32_t>(
			{3, 3, 4}, numerical_type::float32, -1.0F);
		read_now(*reader, destination, regions);
		REQUIRE( get_values<float32_t>(destination) ==
			std::vector<float32_t>(36, -1.0F) );
	}
}
