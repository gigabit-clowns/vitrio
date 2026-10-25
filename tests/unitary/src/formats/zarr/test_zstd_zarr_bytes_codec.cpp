// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zstd_zarr_bytes_codec.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include "zarr_codec_test_bytes.hpp"

#include <zstd.h>

#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

namespace
{

std::vector<byte> compress(const std::vector<byte> &bytes)
{
	std::vector<byte> encoded(ZSTD_compressBound(bytes.size()));
	const auto size = ZSTD_compress(
		encoded.data(), encoded.size(), bytes.data(), bytes.size(), 3);
	REQUIRE_FALSE( ZSTD_isError(size) );
	encoded.resize(size);
	return encoded;
}

// A frame streamed out without its size, as a compressor that did not know
// it up front writes one.
std::vector<byte> compress_unsized(const std::vector<byte> &bytes)
{
	ZSTD_CStream *const stream = ZSTD_createCStream();
	REQUIRE( stream != nullptr );
	ZSTD_initCStream(stream, 3);

	std::vector<byte> encoded(ZSTD_compressBound(bytes.size()) + 64);
	ZSTD_inBuffer input = {bytes.data(), bytes.size(), 0};
	ZSTD_outBuffer output = {encoded.data(), encoded.size(), 0};
	while (input.pos < input.size)
	{
		REQUIRE_FALSE( ZSTD_isError(
			ZSTD_compressStream(stream, &output, &input)) );
	}
	REQUIRE( ZSTD_endStream(stream, &output) == 0 );
	ZSTD_freeCStream(stream);

	encoded.resize(output.pos);
	return encoded;
}

} // anonymous namespace

TEST_CASE( "zstd_zarr_bytes_codec decodes zstd frames",
	"[zstd_zarr_bytes_codec]" )
{
	const zstd_zarr_bytes_codec codec;
	const auto bytes = make_codec_test_bytes(300000);
	std::vector<byte> decoded = {9};

	SECTION( "a frame that states its size" )
	{
		const auto encoded = compress(bytes);
		REQUIRE( ZSTD_getFrameContentSize(encoded.data(), encoded.size()) ==
			bytes.size() );

		codec.decode(as_span(encoded), bytes.size(), decoded);
		REQUIRE( decoded == bytes );
	}

	SECTION( "a frame that does not state its size" )
	{
		const auto encoded = compress_unsized(bytes);
		REQUIRE( ZSTD_getFrameContentSize(encoded.data(), encoded.size()) ==
			ZSTD_CONTENTSIZE_UNKNOWN );

		codec.decode(as_span(encoded), bytes.size(), decoded);
		REQUIRE( decoded == bytes );
	}

	SECTION( "two frames, one after the other" )
	{
		const std::vector<byte> first(bytes.begin(), bytes.begin() + 1000);
		const std::vector<byte> second(bytes.begin() + 1000, bytes.end());
		auto encoded = compress(first);
		const auto rest = compress(second);
		encoded.insert(encoded.end(), rest.begin(), rest.end());

		codec.decode(as_span(encoded), bytes.size(), decoded);
		REQUIRE( decoded == bytes );
	}

	SECTION( "more bytes than the limit are refused, sized or not" )
	{
		REQUIRE_THROWS_AS(
			codec.decode(as_span(compress(bytes)), bytes.size() - 1, decoded),
			image_file_format_error
		);
		REQUIRE_THROWS_AS(
			codec.decode(
				as_span(compress_unsized(bytes)), bytes.size() - 1, decoded),
			image_file_format_error
		);
	}

	SECTION( "bytes that are not zstd are refused" )
	{
		REQUIRE_THROWS_AS(
			codec.decode(as_span(bytes), bytes.size(), decoded),
			image_file_format_error
		);
	}

	SECTION( "a truncated frame is refused" )
	{
		auto encoded = compress_unsized(bytes);
		encoded.resize(encoded.size() - 10);
		REQUIRE_THROWS_AS(
			codec.decode(as_span(encoded), bytes.size(), decoded),
			image_file_format_error
		);
	}
}
