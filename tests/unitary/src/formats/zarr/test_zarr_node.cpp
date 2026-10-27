// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_node.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include "zarr_test_store.hpp"

using namespace vitrio;
using namespace vitrio::zarr;
using nlohmann::json;

TEST_CASE( "read_zarr_node tells what a directory is", "[zarr_node]" )
{
	const scoped_directory directory("test_zarr_node_store");
	boost::filesystem::create_directories(directory.get());

	SECTION( "an array of version 3, its attributes apart" )
	{
		write_store_file(directory.get(), "zarr.json",
			R"({"zarr_format": 3, "node_type": "array", "shape": [2],
				"attributes": {"a": 1}})");
		REQUIRE( has_zarr_metadata(directory.get()) );

		const auto node = read_zarr_node(directory.get());
		REQUIRE( node.kind == zarr_node_kind::array );
		REQUIRE( node.version == 3 );
		REQUIRE( node.metadata_path == directory.get("zarr.json") );
		REQUIRE( node.attributes == json::parse(R"({"a": 1})") );
		REQUIRE_FALSE( node.metadata.contains("attributes") );
		REQUIRE( node.metadata["shape"] == json::parse("[2]") );
	}

	SECTION( "a group of version 3" )
	{
		write_store_file(directory.get(), "zarr.json",
			R"({"zarr_format": 3, "node_type": "group"})");

		const auto node = read_zarr_node(directory.get());
		REQUIRE( node.kind == zarr_node_kind::group );
		REQUIRE( node.attributes == json::object() );
	}

	SECTION( "an array of version 2, with its attributes" )
	{
		write_store_file(directory.get(), ".zarray", R"({"zarr_format": 2})");
		write_store_file(directory.get(), ".zattrs", R"({"b": [1]})");

		const auto node = read_zarr_node(directory.get());
		REQUIRE( node.kind == zarr_node_kind::array );
		REQUIRE( node.version == 2 );
		REQUIRE( node.attributes == json::parse(R"({"b": [1]})") );
	}

	SECTION( "a group of version 2, without attributes" )
	{
		write_store_file(directory.get(), ".zgroup", R"({"zarr_format": 2})");

		const auto node = read_zarr_node(directory.get());
		REQUIRE( node.kind == zarr_node_kind::group );
		REQUIRE( node.attributes == json::object() );
	}

	SECTION( "a directory that is no node" )
	{
		REQUIRE_FALSE( has_zarr_metadata(directory.get()) );
		REQUIRE( read_zarr_node(directory.get()).kind ==
			zarr_node_kind::none );
	}

	SECTION( "a document of another version than its name is malformed" )
	{
		write_store_file(directory.get(), ".zarray", R"({"zarr_format": 3})");
		REQUIRE_THROWS_AS(
			read_zarr_node(directory.get()), image_file_format_error);
	}

	SECTION( "a node of no known kind is malformed" )
	{
		write_store_file(directory.get(), "zarr.json",
			R"({"zarr_format": 3, "node_type": "other"})");
		REQUIRE_THROWS_AS(
			read_zarr_node(directory.get()), image_file_format_error);
	}

	SECTION( "attributes that are no object are malformed" )
	{
		write_store_file(directory.get(), ".zgroup", R"({"zarr_format": 2})");
		write_store_file(directory.get(), ".zattrs", "[1]");
		REQUIRE_THROWS_AS(
			read_zarr_node(directory.get()), image_file_format_error);
	}
}

TEST_CASE( "join_zarr_path joins a directory and a name", "[zarr_node]" )
{
	REQUIRE( join_zarr_path("a/b", "c/0/1") ==
		(boost::filesystem::path("a/b") / "c/0/1").string() );
}
