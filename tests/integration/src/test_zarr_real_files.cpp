// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/file_image_reader_provider.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_file_read_format.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/image_location.hpp>
#include <vitrio/image_read.hpp>

#include <vitrio/tests/assets.hpp>
#include <vitrio/tests/file_transfer_pools.hpp>
#include <vitrio/tests/host_array.hpp>

#include <cstdint>
#include <memory>
#include <vector>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

using extents = std::vector<std::size_t>;

std::shared_ptr<file_image_reader_provider> make_readers()
{
	return std::make_shared<file_image_reader_provider>(
		image_file_read_format_selector::get_shared(),
		make_file_transfer_pool()
	);
}

} // anonymous namespace

TEST_CASE( "the bundled formats read the ZARR stores of the assets",
	"[zarr]" )
{
	const auto selector = image_file_read_format_selector::get_shared();

	SECTION( "a store is claimed by ZARR, with or without a separator" )
	{
		for (const auto *name : {
			"v2_float32_blosc_lz4.zarr", "v3_uint16_big_endian_gzip.zarr/",
			"ome_v05_tilt_series.zarr",
		})
		{
			const auto *format = selector->get_most_suitable_format(
				image_file_probe(get_zarr_asset_path(name)));
			REQUIRE( format != nullptr );
			REQUIRE( format->get_name() == "ZARR" );
		}
	}

	SECTION( "a tomogram as the CryoET Data Portal stores it" )
	{
		const auto values = vitrio::read(
			image_location(get_zarr_asset_path("v2_float32_blosc_lz4.zarr")),
			*make_readers(),
			numerical_type::float64
		);
		REQUIRE( values.get_descriptor().get_extents().size() == 3 );

		const auto read = get_values<double>(values);
		REQUIRE( read.size() == 210 );
		for (std::size_t p = 0; p < read.size(); ++p)
		{
			REQUIRE( read[p] == 0.25 * static_cast<double>(p) - 4 );
		}
	}

	SECTION( "an image of a tilt series, by its index in the stack" )
	{
		const auto image = vitrio::read(
			image_location(get_zarr_asset_path("ome_v05_tilt_series.zarr"), 3),
			*make_readers()
		);
		const auto extents_read = image.get_descriptor().get_extents();
		REQUIRE( extents(extents_read.begin(), extents_read.end()) ==
			extents{8, 8} );

		const auto values = get_values<std::uint16_t>(image);
		for (std::size_t p = 0; p < values.size(); ++p)
		{
			REQUIRE( values[p] == 3 * 64 + p );
		}
	}

	SECTION( "a tomogram is no stack, and a level of it neither" )
	{
		const auto readers = make_readers();
		for (const auto *name : {
			"ome_v04_tomogram.zarr", "ome_v04_tomogram.zarr/1",
		})
		{
			const auto reader = readers->acquire(get_zarr_asset_path(name));
			REQUIRE( reader->get_descriptor().get_core_rank() == 3 );
		}
	}
}
