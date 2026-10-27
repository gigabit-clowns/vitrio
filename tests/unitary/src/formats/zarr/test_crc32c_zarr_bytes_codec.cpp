// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/crc32c_zarr_bytes_codec.hpp>

#include <formats/zarr/crc32c.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

#include "../../compression/compression_test_bytes.hpp"

#include <cstdint>
#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;

namespace
{

std::vector<byte> with_checksum(const std::vector<byte> &bytes)
{
	const auto checksum = compute_crc32c(as_span(bytes));
	auto encoded = bytes;
	for (int i = 0; i < 4; ++i)
	{
		encoded.push_back(static_cast<byte>(checksum >> (8 * i)));
	}

	return encoded;
}

} // anonymous namespace

TEST_CASE( "crc32c_zarr_bytes_codec checks and strips the checksum",
	"[crc32c_zarr_bytes_codec]" )
{
	const crc32c_zarr_bytes_codec codec;
	const auto bytes = make_codec_test_bytes(1000);
	auto encoded = with_checksum(bytes);
	std::vector<byte> decoded = {1, 2, 3};

	SECTION( "bytes whose checksum matches come back without it" )
	{
		codec.decode(as_span(encoded), bytes.size(), decoded);
		REQUIRE( decoded == bytes );
	}

	SECTION( "a changed byte is refused" )
	{
		encoded[10] ^= 1;
		REQUIRE_THROWS_AS(
			codec.decode(as_span(encoded), bytes.size(), decoded),
			image_file_format_error
		);
	}

	SECTION( "a changed checksum is refused" )
	{
		encoded.back() ^= 0x80;
		REQUIRE_THROWS_AS(
			codec.decode(as_span(encoded), bytes.size(), decoded),
			image_file_format_error
		);
	}

	SECTION( "fewer bytes than a checksum are refused" )
	{
		const std::vector<byte> short_chunk = {1, 2, 3};
		REQUIRE_THROWS_AS(
			codec.decode(as_span(short_chunk), 100, decoded),
			image_file_format_error
		);
	}

	SECTION( "more bytes than the limit are refused" )
	{
		REQUIRE_THROWS_AS(
			codec.decode(as_span(encoded), bytes.size() - 1, decoded),
			image_file_format_error
		);
	}

	SECTION( "a checksum of nothing is nothing" )
	{
		const auto empty = with_checksum({});
		codec.decode(as_span(empty), 0, decoded);
		REQUIRE( decoded.empty() );
	}
}

TEST_CASE( "crc32c_zarr_bytes_codec appends the checksum it checks",
	"[crc32c_zarr_bytes_codec]" )
{
	const crc32c_zarr_bytes_codec codec;
	const auto bytes = make_codec_test_bytes(100);

	std::vector<byte> encoded;
	codec.encode(as_span(bytes), encoded);
	REQUIRE( encoded == with_checksum(bytes) );
}
