// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <compression/gzip_identification.hpp>

#include "compression_test_bytes.hpp"
#include "zlib_test_streams.hpp"

#include <algorithm>
#include <vector>

using namespace vitrio;

TEST_CASE( "has_gzip_signature tells a gzip stream by how it begins",
	"[gzip_identification]" )
{
	const auto bytes = make_codec_test_bytes(100);

	CHECK( has_gzip_signature(as_span(deflate_stream(bytes, 31))) );
	CHECK_FALSE( has_gzip_signature(as_span(deflate_stream(bytes, 15))) );
	CHECK_FALSE( has_gzip_signature(as_span(bytes)) );

	const std::vector<byte> short_signature = {0x1f, 0x8b};
	CHECK_FALSE( has_gzip_signature(as_span(short_signature)) );
	CHECK_FALSE( has_gzip_signature(span<const byte>()) );
}

TEST_CASE( "get_gzip_inner_extension names what a path holds compressed",
	"[gzip_identification]" )
{
	CHECK( get_gzip_inner_extension("emd_3197.map.gz") == ".map" );
	CHECK( get_gzip_inner_extension("/data/EMD-3197.MAP.GZ") == ".map" );
	CHECK( get_gzip_inner_extension("stack.mrcs.gz") == ".mrcs" );
	CHECK( get_gzip_inner_extension("a.b.mrc.gz") == ".mrc" );

	SECTION( "a path that does not end in .gz has none" )
	{
		CHECK( get_gzip_inner_extension("emd_3197.map").empty() );
		CHECK( get_gzip_inner_extension("emd_3197.map.gzip").empty() );
		CHECK( get_gzip_inner_extension("").empty() );
	}

	SECTION( "a name with no other extension has none" )
	{
		CHECK( get_gzip_inner_extension("volume.gz").empty() );
		CHECK( get_gzip_inner_extension(".gz").empty() );
		CHECK( get_gzip_inner_extension(".map.gz").empty() );
		CHECK( get_gzip_inner_extension("dir.map/volume.gz").empty() );
	}
}

TEST_CASE( "inflate_leading_bytes inflates as far as the bytes go",
	"[gzip_identification]" )
{
	const auto bytes = make_codec_test_bytes(100000);
	const auto encoded = deflate_stream(bytes, 31);

	SECTION( "up to the limit" )
	{
		const auto inflated = inflate_leading_bytes(as_span(encoded), 1024);
		REQUIRE( inflated.size() == 1024 );
		REQUIRE( std::equal(inflated.begin(), inflated.end(), bytes.begin()) );
	}

	SECTION( "up to where a stream cut short ends" )
	{
		const std::vector<byte> leading(encoded.begin(), encoded.begin() + 600);
		const auto inflated = inflate_leading_bytes(as_span(leading), 1 << 20);
		REQUIRE_FALSE( inflated.empty() );
		REQUIRE( inflated.size() < bytes.size() );
		REQUIRE( std::equal(inflated.begin(), inflated.end(), bytes.begin()) );
	}

	SECTION( "up to where the stream breaks off" )
	{
		// Every byte inflates before the checksum is found not to match.
		auto broken = deflate_stream(make_codec_test_bytes(500), 31);
		broken[broken.size() - 8] ^= 0xff;
		const auto inflated = inflate_leading_bytes(as_span(broken), 1 << 20);
		REQUIRE( inflated == make_codec_test_bytes(500) );
	}

	SECTION( "nothing of bytes that are not gzip" )
	{
		REQUIRE( inflate_leading_bytes(as_span(bytes), 1024).empty() );
		REQUIRE( inflate_leading_bytes(
			as_span(deflate_stream(bytes, 15)), 1024).empty() );
	}
}
