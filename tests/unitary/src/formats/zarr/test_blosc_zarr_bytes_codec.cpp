// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <formats/zarr/blosc_zarr_bytes_codec.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include "../../compression/compression_test_bytes.hpp"

#include <blosc.h>

#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

namespace
{

std::vector<byte> compress(
	const std::vector<byte> &bytes,
	const char *compressor,
	int shuffle,
	std::size_t type_size
)
{
	std::vector<byte> encoded(bytes.size() + BLOSC_MAX_OVERHEAD);
	const auto size = blosc_compress_ctx(
		5, shuffle, type_size, bytes.size(), bytes.data(),
		encoded.data(), encoded.size(), compressor, 0, 1);
	REQUIRE( size > 0 );
	encoded.resize(static_cast<std::size_t>(size));
	return encoded;
}

} // anonymous namespace

TEST_CASE( "blosc_zarr_bytes_codec decodes what blosc encodes",
	"[blosc_zarr_bytes_codec]" )
{
	const blosc_zarr_bytes_codec codec;
	const auto bytes = make_codec_test_bytes(100000);
	std::vector<byte> decoded;

	SECTION( "every compressor zarr writes with, shuffled or not" )
	{
		const std::string compressor =
			GENERATE("blosclz", "lz4", "lz4hc", "zlib", "zstd");
		const int shuffle = GENERATE(
			BLOSC_NOSHUFFLE, BLOSC_SHUFFLE, BLOSC_BITSHUFFLE);

		const auto encoded = compress(bytes, compressor.c_str(), shuffle, 4);
		codec.decode(as_span(encoded), bytes.size(), decoded);

		REQUIRE( decoded == bytes );
	}

	SECTION( "a chunk that decodes into more than the limit is refused" )
	{
		const auto encoded = compress(bytes, "lz4", BLOSC_SHUFFLE, 4);
		REQUIRE_THROWS_AS(
			codec.decode(as_span(encoded), bytes.size() - 1, decoded),
			image_file_format_error
		);
	}

	SECTION( "bytes that are not blosc are refused" )
	{
		REQUIRE_THROWS_AS(
			codec.decode(as_span(bytes), bytes.size(), decoded),
			image_file_format_error
		);
	}

	SECTION( "a truncated chunk is refused" )
	{
		auto encoded = compress(bytes, "zstd", BLOSC_SHUFFLE, 4);
		encoded.resize(encoded.size() / 2);
		REQUIRE_THROWS_AS(
			codec.decode(as_span(encoded), bytes.size(), decoded),
			image_file_format_error
		);
	}

	SECTION( "fewer bytes than a header are refused" )
	{
		const std::vector<byte> tiny = {2, 1, 0};
		REQUIRE_THROWS_AS(
			codec.decode(as_span(tiny), bytes.size(), decoded),
			image_file_format_error
		);
	}
}

TEST_CASE( "blosc_zarr_bytes_codec encodes what it decodes",
	"[blosc_zarr_bytes_codec]" )
{
	const auto bytes = make_codec_test_bytes(100000);
	const std::string compressor = GENERATE("lz4", "zstd", "zlib");
	const blosc_zarr_bytes_codec codec(compressor, 5, 1, 4);

	std::vector<byte> encoded;
	codec.encode(as_span(bytes), encoded);
	REQUIRE( encoded.size() < bytes.size() );

	std::vector<byte> decoded;
	codec.decode(as_span(encoded), bytes.size(), decoded);
	REQUIRE( decoded == bytes );

	std::size_t size = 0;
	REQUIRE( blosc_cbuffer_validate(
		encoded.data(), encoded.size(), &size) >= 0 );
	REQUIRE( size == bytes.size() );
}
