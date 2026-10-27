// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_store_metadata.hpp>

#include <formats/zarr/zarr_array_layout.hpp>
#include <formats/zarr/zarr_core_rank.hpp>
#include <formats/zarr/zarr_multiscale.hpp>
#include <formats/zarr/zarr_node.hpp>

#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_descriptor.hpp>

#include <cstddef>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

namespace
{

using extents = std::vector<std::size_t>;
using names = std::vector<std::string>;

image_descriptor describe(
	const extents &shape,
	std::size_t core_rank,
	numerical_type type = numerical_type::float32
)
{
	return image_descriptor(make_span(shape), core_rank, type);
}

zarr_node as_array_node(const nlohmann::json &metadata)
{
	zarr_node node;
	node.kind = zarr_node_kind::array;
	node.version = 3;
	node.metadata_path = "test.zarr/zarr.json";
	node.metadata = metadata;
	node.attributes = metadata.at("attributes");
	node.metadata.erase("attributes");
	return node;
}

} // anonymous namespace

TEST_CASE( "get_v3_data_type_name names every type ZARR has",
	"[zarr_store_metadata]" )
{
	REQUIRE( get_v3_data_type_name(numerical_type::boolean) == "bool" );
	REQUIRE( get_v3_data_type_name(numerical_type::int8) == "int8" );
	REQUIRE( get_v3_data_type_name(numerical_type::uint64) == "uint64" );
	REQUIRE( get_v3_data_type_name(numerical_type::float16) == "float16" );
	REQUIRE( get_v3_data_type_name(numerical_type::float32) == "float32" );
	REQUIRE( get_v3_data_type_name(numerical_type::complex_float32) ==
		"complex64" );
	REQUIRE( get_v3_data_type_name(numerical_type::complex_float64) ==
		"complex128" );
	REQUIRE_THROWS_AS(
		get_v3_data_type_name(numerical_type::complex_float16),
		unsupported_operation_error
	);
}

TEST_CASE( "choose_chunk_extents chunks each image or volume on its own",
	"[zarr_store_metadata]" )
{
	SECTION( "a stack of images, in squares one image deep" )
	{
		REQUIRE( choose_chunk_extents(describe({41, 4096, 4096}, 2), 1024,
			128) == extents({1, 1024, 1024}) );
	}

	SECTION( "a volume, in cubes" )
	{
		REQUIRE( choose_chunk_extents(describe({300, 300, 300}, 3), 1024,
			128) == extents({128, 128, 128}) );
	}

	SECTION( "narrower than a chunk, as wide as the image" )
	{
		REQUIRE( choose_chunk_extents(describe({2, 50, 3}, 3), 1024, 128) ==
			extents({2, 50, 3}) );
		REQUIRE( choose_chunk_extents(describe({7, 20}, 2), 1024, 128) ==
			extents({7, 20}) );
	}

	SECTION( "a stack of volumes, many axes deep" )
	{
		REQUIRE( choose_chunk_extents(describe({2, 3, 200, 10, 10}, 3), 1024,
			128) == extents({1, 1, 128, 10, 10}) );
	}

	SECTION( "an edge of zero is taken as one" )
	{
		REQUIRE( choose_chunk_extents(describe({4, 4}, 2), 0, 0) ==
			extents({1, 1}) );
	}
}

TEST_CASE( "name_axes names the stack n and the image z, y and x",
	"[zarr_store_metadata]" )
{
	REQUIRE( name_axes(describe({4, 4}, 2)) == names({"y", "x"}) );
	REQUIRE( name_axes(describe({4, 4, 4}, 3)) == names({"z", "y", "x"}) );
	REQUIRE( name_axes(describe({5, 4, 4}, 2)) == names({"n", "y", "x"}) );
	REQUIRE( name_axes(describe({2, 5, 4, 4}, 2)) ==
		names({"n0", "n1", "y", "x"}) );
	REQUIRE( name_axes(describe({7}, 1)) == names({"x"}) );
	REQUIRE( name_axes(describe({2, 2, 2, 2}, 4)) ==
		names({"a0", "z", "y", "x"}) );
}

TEST_CASE( "is_ome_image accepts what OME-Zarr can state",
	"[zarr_store_metadata]" )
{
	REQUIRE( is_ome_image(describe({4, 4}, 2)) );
	REQUIRE( is_ome_image(describe({4, 4, 4}, 3)) );
	REQUIRE( is_ome_image(describe({2, 2, 4, 4, 4}, 3)) );
	REQUIRE_FALSE( is_ome_image(describe({7}, 1)) );
	REQUIRE_FALSE( is_ome_image(describe({3, 7}, 1)) );
	REQUIRE_FALSE( is_ome_image(describe({2, 2, 2, 2}, 4)) );
	REQUIRE_FALSE( is_ome_image(describe({2, 2, 2, 2, 2, 2}, 3)) );
}

TEST_CASE( "make_array_metadata makes what the reader reads back",
	"[zarr_store_metadata]" )
{
	const auto descriptor = describe({5, 40, 30}, 2, numerical_type::uint16);
	const auto metadata = make_array_metadata(descriptor, {1, 32, 30});

	REQUIRE( metadata["zarr_format"] == 3 );
	REQUIRE( metadata["node_type"] == "array" );
	REQUIRE( metadata["data_type"] == "uint16" );
	REQUIRE( metadata["fill_value"] == 0 );
	REQUIRE( metadata["dimension_names"] == nlohmann::json({"n", "y", "x"}) );
	REQUIRE( metadata["attributes"]["vitrio"]["core_rank"] == 2 );
	REQUIRE( metadata["codecs"][1]["name"] == "blosc" );
	REQUIRE( metadata["codecs"][1]["configuration"]["typesize"] == 2 );

	const zarr_array_layout layout(as_array_node(metadata));
	const auto shape = layout.get_shape();
	REQUIRE( extents(shape.begin(), shape.end()) == extents({5, 40, 30}) );
	const auto chunk = layout.get_chunk_layout().get_extents();
	REQUIRE( extents(chunk.begin(), chunk.end()) == extents({1, 32, 30}) );
	REQUIRE( layout.get_chunk_layout().get_data_type() ==
		numerical_type::uint16 );
	REQUIRE( layout.get_chunk_layout().get_byte_order() ==
		byte_order::little_endian );
	REQUIRE( layout.get_shard_layout() == nullptr );
	REQUIRE( layout.get_key_encoding().get_key(make_span(extents{4, 1, 0}))
		== "c/4/1/0" );

	REQUIRE( decide_core_rank(3, metadata["attributes"],
		layout.get_dimension_names(), nullptr) == 2 );
}

TEST_CASE( "make_array_metadata fills with the zero of each type",
	"[zarr_store_metadata]" )
{
	const extents shape = {2, 2};
	const auto chunk = extents{2, 2};
	for (const auto type : {
		numerical_type::boolean, numerical_type::int8,
		numerical_type::float16, numerical_type::float64,
		numerical_type::complex_float32, numerical_type::complex_float64,
	})
	{
		const auto metadata = make_array_metadata(
			describe(shape, 2, type), chunk);
		const zarr_array_layout layout(as_array_node(metadata));
		const auto fill = layout.get_fill_element();
		REQUIRE( fill.size() == get_size(type) );
		for (const auto value : fill)
		{
			REQUIRE( value == byte(0) );
		}
	}
}

TEST_CASE( "make_ome_group_metadata makes an image of OME-Zarr 0.5",
	"[zarr_store_metadata]" )
{
	const auto metadata = make_ome_group_metadata(
		describe({5, 40, 30}, 2));

	REQUIRE( metadata["zarr_format"] == 3 );
	REQUIRE( metadata["node_type"] == "group" );
	const auto &ome = metadata["attributes"]["ome"];
	REQUIRE( ome["version"] == "0.5" );

	const auto &image = ome["multiscales"].at(0);
	REQUIRE( image["axes"].size() == 3 );
	REQUIRE( image["axes"][0] == nlohmann::json({{"name", "n"}}) );
	REQUIRE( image["axes"][1] ==
		nlohmann::json({{"name", "y"}, {"type", "space"}}) );
	REQUIRE( image["axes"][2] ==
		nlohmann::json({{"name", "x"}, {"type", "space"}}) );
	REQUIRE( image["datasets"].size() == 1 );
	REQUIRE( image["datasets"][0]["path"] == "0" );
	REQUIRE( image["datasets"][0]["coordinateTransformations"][0]["scale"] ==
		nlohmann::json({1.0, 1.0, 1.0}) );

	const auto multiscale = parse_multiscale(
		metadata["attributes"], "test.zarr");
	REQUIRE( multiscale.is_initialized() );
	REQUIRE( multiscale->axes.size() == 3 );
	REQUIRE( multiscale->axes[0].type.empty() );
	REQUIRE( multiscale->axes[2].type == "space" );
	REQUIRE( multiscale->datasets.size() == 1 );
	REQUIRE( decide_core_rank(3, nlohmann::json::object(), {},
		&*multiscale) == 2 );
}
