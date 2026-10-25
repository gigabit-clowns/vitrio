// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief Inflate one deflate stream, as zlib wraps it for the zlib and the
 * gzip codecs.
 *
 * @param encoded The stream. Nothing may follow it.
 * @param window_bits What zlib's inflateInit2 is given: 15 for a zlib
 * stream, 31 for a gzip one.
 * @param limit Most bytes the stream may inflate into.
 * @param decoded Where the bytes are written, replacing what it held.
 * @param codec The name of the codec, which the messages start with.
 * @throws image_file_format_error If @p encoded is not one whole stream of
 * its kind, or inflates into more than @p limit bytes.
 */
void inflate_stream(
	span<const byte> encoded,
	int window_bits,
	std::size_t limit,
	std::vector<byte> &decoded,
	const char *codec
);

} // namespace zarr
} // namespace vitrio
