// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <vector>

namespace vitrio
{

/**
 * @brief Deflate bytes into one stream, as zlib wraps it for the zlib and the
 * gzip codecs.
 *
 * A gzip stream is written with no name and no time, so that the same bytes
 * deflate into the same stream.
 *
 * @param decoded The bytes.
 * @param window_bits What zlib's deflateInit2 is given: 15 for a zlib
 * stream, 31 for a gzip one.
 * @param level How hard it compresses, from 0 to 9.
 * @param encoded Where the stream is written, replacing what it held.
 * @param codec The name of the codec, which the messages start with.
 * @throws std::runtime_error If zlib fails.
 */
void deflate_stream(
	span<const byte> decoded,
	int window_bits,
	int level,
	std::vector<byte> &encoded,
	const char *codec
);

} // namespace vitrio
