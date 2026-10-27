// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/zarr/zarr_array_layout.hpp>

#include <array/fixed_width_float.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>

#include <cstring>
#include <stdexcept>
#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;
using nlohmann::json;

namespace
{

using extents = std::vector<std::size_t>;
using strides = std::vector<std::ptrdiff_t>;

zarr_node make_node(int version, const char *metadata)
{
	zarr_node node;
	node.kind = zarr_node_kind::array;
	node.version = version;
	node.metadata_path = version == 2 ? "store/.zarray" : "store/zarr.json";
	node.metadata = json::parse(metadata);
	return node;
}

// The metadata of an array of version 2, with what a case changes.
zarr_node make_v2(const json &changes)
{
	auto node = make_node(2, R"({
		"zarr_format": 2, "shape": [10, 20], "chunks": [4, 8],
		"dtype": "<f4", "order": "C", "fill_value": 1.5,
		"compressor": {"id": "blosc", "cname": "lz4", "clevel": 5,
			"shuffle": 1, "blocksize": 0},
		"filters": null
	})");
	node.metadata.update(changes);
	return node;
}

zarr_node make_v3(const json &changes)
{
	auto node = make_node(3, R"({
		"zarr_format": 3, "node_type": "array", "shape": [10, 20, 30],
		"data_type": "uint16",
		"chunk_grid": {"name": "regular",
			"configuration": {"chunk_shape": [2, 4, 8]}},
		"chunk_key_encoding": {"name": "default"},
		"fill_value": 7,
		"codecs": [{"name": "bytes", "configuration": {"endian": "big"}},
			{"name": "zstd", "configuration": {"level": 3}},
			{"name": "crc32c"}]
	})");
	node.metadata.update(changes);
	return node;
}

extents shape_of(const zarr_array_layout &layout)
{
	const auto values = layout.get_shape();
	return extents(values.begin(), values.end());
}

extents chunk_of(const zarr_array_layout &layout)
{
	const auto values = layout.get_chunk_layout().get_extents();
	return extents(values.begin(), values.end());
}

strides strides_of(const zarr_array_layout &layout)
{
	const auto values = layout.get_chunk_layout().get_strides();
	return strides(values.begin(), values.end());
}

std::string key_of(const zarr_array_layout &layout, const extents &chunk)
{
	return layout.get_key_encoding().get_key(make_span(chunk));
}

} // anonymous namespace

TEST_CASE( "zarr_array_layout reads an array of version 2",
	"[zarr_array_layout]" )
{
	SECTION( "its shape, chunks, type, fill value, codecs and keys" )
	{
		const zarr_array_layout layout(make_v2(json::object()));

		REQUIRE( shape_of(layout) == extents{10, 20} );
		REQUIRE( chunk_of(layout) == extents{4, 8} );
		REQUIRE( strides_of(layout) == strides{8, 1} );
		REQUIRE( layout.get_chunk_layout().get_data_type() ==
			numerical_type::float32 );
		REQUIRE( layout.get_chunk_layout().get_byte_order() ==
			byte_order::little_endian );
		REQUIRE( layout.get_codecs().get_codec_count() == 1 );
		REQUIRE( key_of(layout, {2, 1}) == "2.1" );
		REQUIRE( layout.get_dimension_names().empty() );

		float32_t fill;
		REQUIRE( layout.get_fill_element().size() == sizeof(fill) );
		std::memcpy(&fill, layout.get_fill_element().data(), sizeof(fill));
		REQUIRE( fill == 1.5F );
	}

	SECTION( "the order F makes the first axis contiguous" )
	{
		const zarr_array_layout layout(make_v2({{"order", "F"}}));
		REQUIRE( strides_of(layout) == strides{1, 4} );
	}

	SECTION( "nested keys, no compressor and an empty list of filters" )
	{
		const zarr_array_layout layout(make_v2({
			{"dimension_separator", "/"},
			{"compressor", nullptr},
			{"filters", json::array()},
		}));
		REQUIRE( key_of(layout, {2, 1}) == "2/1" );
		REQUIRE( layout.get_codecs().is_empty() );
	}

	SECTION( "every compressor it reads" )
	{
		for (const auto *id : {"blosc", "zstd", "zlib", "gzip"})
		{
			const zarr_array_layout layout(
				make_v2({{"compressor", {{"id", id}}}}));
			REQUIRE( layout.get_codecs().get_codec_count() == 1 );
		}
	}

	SECTION( "what it does not read is refused as unsupported" )
	{
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v2({{"compressor", {{"id", "lz4"}}}})),
			unsupported_operation_error);
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v2(
				{{"filters", json::parse(R"([{"id": "delta"}])")}})),
			unsupported_operation_error);
	}

	SECTION( "what ZARR does not state is malformed, naming the document" )
	{
		REQUIRE_THROWS_MATCHES(
			zarr_array_layout(make_v2({{"chunks", {4}}})),
			image_file_format_error,
			Catch::Matchers::MessageMatches(
				Catch::Matchers::StartsWith("store/.zarray: "))
		);
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v2({{"chunks", {4, 0}}})),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v2({{"order", "K"}})),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v2({{"fill_value", "x"}})),
			image_file_format_error);

		auto missing = make_v2(json::object());
		missing.metadata.erase("dtype");
		REQUIRE_THROWS_AS(
			zarr_array_layout(missing), image_file_format_error);
	}

	SECTION( "chunks too large to count are malformed" )
	{
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v2({{"chunks",
				json::parse("[4611686018427387904, 8]")}})),
			image_file_format_error);
	}
}

TEST_CASE( "zarr_array_layout reads an array of version 3",
	"[zarr_array_layout]" )
{
	SECTION( "its codecs, with the order of its bytes" )
	{
		const zarr_array_layout layout(make_v3(json::object()));

		REQUIRE( shape_of(layout) == extents{10, 20, 30} );
		REQUIRE( chunk_of(layout) == extents{2, 4, 8} );
		REQUIRE( strides_of(layout) == strides{32, 8, 1} );
		REQUIRE( layout.get_chunk_layout().get_data_type() ==
			numerical_type::uint16 );
		REQUIRE( layout.get_chunk_layout().get_byte_order() ==
			byte_order::big_endian );
		REQUIRE( layout.get_codecs().get_codec_count() == 2 );
		REQUIRE( key_of(layout, {1, 2, 3}) == "c/1/2/3" );
	}

	SECTION( "transposes, one after another" )
	{
		const zarr_array_layout layout(make_v3({{"codecs", json::parse(R"([
			{"name": "transpose", "configuration": {"order": [2, 0, 1]}},
			{"name": "transpose", "configuration": {"order": [1, 0, 2]}},
			{"name": "bytes", "configuration": {"endian": "little"}}
		])")}}));

		// After both, memory takes the axes 0, 2, 1 of the array, slowest
		// first: [2, 0, 1] then positions 1, 0, 2 of that.
		REQUIRE( strides_of(layout) == strides{32, 1, 4} );
	}

	SECTION( "a type of one byte needs no order of its bytes" )
	{
		const zarr_array_layout layout(make_v3({
			{"data_type", "uint8"},
			{"codecs", json::parse(R"([{"name": "bytes"}])")},
		}));
		REQUIRE( layout.get_codecs().is_empty() );
	}

	SECTION( "a wider one does" )
	{
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v3(
				{{"codecs", json::parse(R"([{"name": "bytes"}])")}})),
			image_file_format_error);
	}

	SECTION( "the names of its dimensions" )
	{
		const zarr_array_layout layout(make_v3(
			{{"dimension_names", json::parse(R"(["z", null, "x"])")}}));
		REQUIRE( layout.get_dimension_names() ==
			std::vector<std::string>{"z", "", "x"} );

		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v3(
				{{"dimension_names", json::parse(R"(["z"])")}})),
			image_file_format_error);
	}

	SECTION( "an extension that need not be understood is passed over" )
	{
		const zarr_array_layout layout(make_v3(
			{{"extra", json::parse(R"({"must_understand": false})")}}));
		REQUIRE( shape_of(layout) == extents{10, 20, 30} );
	}

	SECTION( "what it does not read is refused as unsupported" )
	{
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v3({{"extra", 1}})),
			unsupported_operation_error);
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v3({{"storage_transformers",
				json::parse(R"([{"name": "x"}])")}})),
			unsupported_operation_error);
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v3({{"chunk_grid", json::parse(
				R"({"name": "rectilinear", "configuration": {}})")}})),
			unsupported_operation_error);
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v3({{"codecs", json::parse(R"([
				{"name": "bytes", "configuration": {"endian": "little"}},
				{"name": "lz4"}
			])")}})),
			unsupported_operation_error);
	}

	SECTION( "codecs out of their order are malformed" )
	{
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v3({{"codecs", json::parse(R"([
				{"name": "zstd"},
				{"name": "bytes", "configuration": {"endian": "little"}}
			])")}})),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v3({{"codecs", json::parse(R"([
				{"name": "bytes", "configuration": {"endian": "little"}},
				{"name": "transpose", "configuration": {"order": [0, 1, 2]}}
			])")}})),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v3(
				{{"codecs", json::parse(R"([{"name": "zstd"}])")}})),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			zarr_array_layout(make_v3({{"codecs", json::parse(R"([
				{"name": "transpose", "configuration": {"order": [0, 0, 1]}},
				{"name": "bytes", "configuration": {"endian": "little"}}
			])")}})),
			image_file_format_error);
	}
}

TEST_CASE( "zarr_array_layout reads a sharded array of version 3",
	"[zarr_array_layout]" )
{
	// Shards of 2 by 4 by 8 of chunks of 1 by 2 by 4.
	const auto sharded = [] (const char *configuration)
	{
		auto codecs = json::array();
		codecs.push_back({
			{"name", "sharding_indexed"},
			{"configuration", json::parse(configuration)},
		});
		return make_v3({{"codecs", codecs}});
	};
	const char *const standard = R"({"chunk_shape": [1, 2, 4],
		"codecs": [{"name": "bytes", "configuration": {"endian": "big"}},
			{"name": "zstd"}],
		"index_codecs": [{"name": "bytes",
			"configuration": {"endian": "little"}}, {"name": "crc32c"}]})";

	SECTION( "a chunk of a shard is what is decoded" )
	{
		const zarr_array_layout layout(sharded(standard));

		REQUIRE( chunk_of(layout) == extents{1, 2, 4} );
		REQUIRE( strides_of(layout) == strides{8, 4, 1} );
		REQUIRE( layout.get_chunk_layout().get_byte_order() ==
			byte_order::big_endian );
		REQUIRE( layout.get_codecs().get_codec_count() == 1 );

		const auto *const shards = layout.get_shard_layout();
		REQUIRE( shards != nullptr );
		const auto per_shard = shards->get_chunks_per_shard();
		REQUIRE( extents(per_shard.begin(), per_shard.end()) ==
			extents{2, 2, 2} );
		REQUIRE( shards->get_index_location() == zarr_index_location::end );
		REQUIRE( shards->get_index_byte_order() ==
			byte_order::little_endian );
		REQUIRE( shards->has_index_checksum() );
		REQUIRE( shards->get_index_byte_size() == 8 * 16 + 4 );
	}

	SECTION( "its index at its start, unchecked" )
	{
		const zarr_array_layout layout(sharded(R"({"chunk_shape": [2, 4, 8],
			"codecs": [{"name": "bytes", "configuration": {"endian": "big"}}],
			"index_codecs": [{"name": "bytes",
				"configuration": {"endian": "big"}}],
			"index_location": "start"})"));

		const auto *const shards = layout.get_shard_layout();
		REQUIRE( shards->get_index_location() ==
			zarr_index_location::start );
		REQUIRE_FALSE( shards->has_index_checksum() );
		REQUIRE( shards->get_index_byte_size() == 16 );
	}

	SECTION( "an array of no shards has no layout of them" )
	{
		REQUIRE( zarr_array_layout(make_v3(json::object()))
			.get_shard_layout() == nullptr );
	}

	SECTION( "what it does not read is refused as unsupported" )
	{
		REQUIRE_THROWS_AS(
			zarr_array_layout(sharded(R"({"chunk_shape": [1, 2, 4],
				"codecs": [{"name": "sharding_indexed", "configuration": {}}],
				"index_codecs": [{"name": "bytes",
					"configuration": {"endian": "little"}}]})")),
			unsupported_operation_error);
		REQUIRE_THROWS_AS(
			zarr_array_layout(sharded(R"({"chunk_shape": [1, 2, 4],
				"codecs": [{"name": "bytes",
					"configuration": {"endian": "big"}}],
				"index_codecs": [{"name": "bytes",
					"configuration": {"endian": "little"}},
					{"name": "zstd"}]})")),
			unsupported_operation_error);

		auto compressed = make_v3({{"codecs", json::parse(R"([
			{"name": "sharding_indexed", "configuration": {}},
			{"name": "zstd"}
		])")}});
		REQUIRE_THROWS_AS(
			zarr_array_layout(compressed), unsupported_operation_error);
	}

	SECTION( "chunks that do not divide a shard are malformed" )
	{
		REQUIRE_THROWS_AS(
			zarr_array_layout(sharded(R"({"chunk_shape": [1, 3, 4],
				"codecs": [{"name": "bytes",
					"configuration": {"endian": "big"}}],
				"index_codecs": [{"name": "bytes",
					"configuration": {"endian": "little"}}]})")),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			zarr_array_layout(sharded(R"({"chunk_shape": [1, 2, 4],
				"codecs": [{"name": "bytes",
					"configuration": {"endian": "big"}}],
				"index_codecs": [{"name": "bytes",
					"configuration": {"endian": "little"}}],
				"index_location": "middle"})")),
			image_file_format_error);
	}
}

TEST_CASE( "zarr_array_layout reads arrays only", "[zarr_array_layout]" )
{
	auto node = make_v2(json::object());
	node.kind = zarr_node_kind::group;
	REQUIRE_THROWS_AS(zarr_array_layout(node), std::invalid_argument);
}
