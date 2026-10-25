// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zlib_zarr_bytes_codec.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include "zarr_codec_test_bytes.hpp"
#include "zlib_test_streams.hpp"

#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

TEST_CASE( "zlib_zarr_bytes_codec decodes zlib streams",
	"[zlib_zarr_bytes_codec]" )
{
	const zlib_zarr_bytes_codec codec;
	const auto bytes = make_codec_test_bytes(10000);
	std::vector<byte> decoded;

	SECTION( "a zlib stream" )
	{
		codec.decode(as_span(deflate_stream(bytes, 15)), bytes.size(), decoded);
		REQUIRE( decoded == bytes );
	}

	SECTION( "a stream of the other kind is refused" )
	{
		REQUIRE_THROWS_AS(
			codec.decode(
				as_span(deflate_stream(bytes, 31)), bytes.size(), decoded),
			image_file_format_error
		);
	}
}
