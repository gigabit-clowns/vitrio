// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <vitrio/byte.hpp>

#include <zlib.h>

#include <vector>

namespace vitrio
{

// Deflates bytes into a zlib stream, for window bits of 15, or a gzip one,
// for 31.
inline std::vector<byte> deflate_stream(
	const std::vector<byte> &bytes,
	int window_bits
)
{
	z_stream stream = {};
	REQUIRE( deflateInit2(&stream, 6, Z_DEFLATED, window_bits, 8,
		Z_DEFAULT_STRATEGY) == Z_OK );

	std::vector<byte> encoded(deflateBound(&stream, bytes.size()) + 32);
	stream.next_in = const_cast<Bytef*>(bytes.data());
	stream.avail_in = static_cast<uInt>(bytes.size());
	stream.next_out = encoded.data();
	stream.avail_out = static_cast<uInt>(encoded.size());
	REQUIRE( deflate(&stream, Z_FINISH) == Z_STREAM_END );
	encoded.resize(stream.total_out);
	deflateEnd(&stream);

	return encoded;
}

} // namespace vitrio
