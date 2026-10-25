// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zlib_deflate.hpp>

#include <formats/zarr/zlib_inflate.hpp>

#include "zarr_codec_test_bytes.hpp"

#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

TEST_CASE( "deflate_stream deflates into one stream", "[zlib_deflate]" )
{
	const auto bytes = make_codec_test_bytes(1000000);
	std::vector<byte> encoded;
	std::vector<byte> decoded;

	SECTION( "of either kind, which inflates back" )
	{
		for (const int window_bits : {15, 31})
		{
			deflate_stream(as_span(bytes), window_bits, 6, encoded, "test");
			inflate_stream(
				as_span(encoded), window_bits, bytes.size(), decoded, "test");
			REQUIRE( decoded == bytes );
		}
	}

	SECTION( "the same, from the same bytes, gzip included" )
	{
		std::vector<byte> again;
		deflate_stream(as_span(bytes), 31, 6, encoded, "test");
		deflate_stream(as_span(bytes), 31, 6, again, "test");
		REQUIRE( encoded == again );
	}

	SECTION( "of nothing" )
	{
		deflate_stream(span<const byte>(), 15, 6, encoded, "test");
		inflate_stream(as_span(encoded), 15, 0, decoded, "test");
		REQUIRE( decoded.empty() );
	}

	SECTION( "bytes it can not compress, which grow" )
	{
		std::vector<byte> noise(5000);
		unsigned state = 12345;
		for (auto &value : noise)
		{
			state = state * 1103515245U + 12345U;
			value = static_cast<byte>(state >> 24);
		}
		deflate_stream(as_span(noise), 31, 9, encoded, "test");
		inflate_stream(as_span(encoded), 31, noise.size(), decoded, "test");
		REQUIRE( decoded == noise );
	}
}
