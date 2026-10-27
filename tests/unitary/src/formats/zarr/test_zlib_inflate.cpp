// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <formats/zarr/zlib_inflate.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include "zarr_codec_test_bytes.hpp"
#include "zlib_test_streams.hpp"

#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

TEST_CASE( "inflate_stream inflates one whole stream", "[zlib_inflate]" )
{
	// More than the output starts at, so that it grows.
	const auto bytes = make_codec_test_bytes(1000000);
	std::vector<byte> decoded;

	SECTION( "a zlib stream" )
	{
		inflate_stream(
			as_span(deflate_stream(bytes, 15)), 15, bytes.size(), decoded,
			"test");
		REQUIRE( decoded == bytes );
	}

	SECTION( "a gzip stream" )
	{
		inflate_stream(
			as_span(deflate_stream(bytes, 31)), 31, bytes.size(), decoded,
			"test");
		REQUIRE( decoded == bytes );
	}

	SECTION( "an empty stream" )
	{
		inflate_stream(
			as_span(deflate_stream({}, 15)), 15, 0, decoded, "test");
		REQUIRE( decoded.empty() );
	}

	SECTION( "a stream of the other kind is refused" )
	{
		REQUIRE_THROWS_AS(
			inflate_stream(
				as_span(deflate_stream(bytes, 31)), 15, bytes.size(),
				decoded, "test"),
			image_file_format_error
		);
	}

	SECTION( "more bytes than the limit are refused" )
	{
		REQUIRE_THROWS_AS(
			inflate_stream(
				as_span(deflate_stream(bytes, 15)), 15, bytes.size() - 1,
				decoded, "test"),
			image_file_format_error
		);
	}

	SECTION( "a truncated stream is refused" )
	{
		auto encoded = deflate_stream(bytes, 15);
		encoded.resize(encoded.size() - 8);
		REQUIRE_THROWS_AS(
			inflate_stream(as_span(encoded), 15, bytes.size(), decoded, "test"),
			image_file_format_error
		);
	}

	SECTION( "bytes after the stream are refused" )
	{
		auto encoded = deflate_stream(bytes, 15);
		encoded.push_back(0);
		REQUIRE_THROWS_AS(
			inflate_stream(as_span(encoded), 15, bytes.size(), decoded, "test"),
			image_file_format_error
		);
	}

	SECTION( "the message starts with the codec" )
	{
		REQUIRE_THROWS_WITH(
			inflate_stream(as_span(bytes), 15, bytes.size(), decoded, "name"),
			Catch::Matchers::StartsWith("name: ")
		);
	}
}
