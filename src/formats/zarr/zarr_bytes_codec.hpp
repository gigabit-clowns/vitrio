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
 * @brief One of the codecs a ZARR array encodes its chunks with that turns
 * bytes into bytes, such as a compressor or a checksum.
 *
 * The codecs that turn an array into another, such as a transpose, and the
 * one that turns it into bytes, which sets their order, say how the values
 * of a decoded chunk are laid out, and are no codecs here: the strided
 * transfer reads a chunk in any layout.
 */
class zarr_bytes_codec
{
public:
	zarr_bytes_codec() noexcept;
	zarr_bytes_codec(const zarr_bytes_codec &other) = delete;
	zarr_bytes_codec(zarr_bytes_codec &&other) = delete;
	virtual ~zarr_bytes_codec();

	zarr_bytes_codec& operator=(const zarr_bytes_codec &other) = delete;
	zarr_bytes_codec& operator=(zarr_bytes_codec &&other) = delete;

	/**
	 * @brief Undo what this codec made of some bytes.
	 *
	 * @param encoded The bytes this codec made.
	 * @param limit Most bytes they may decode into. They come from a file,
	 * which could otherwise ask for any amount of memory.
	 * @param decoded Where the bytes are written, replacing what it held.
	 * @throws image_file_format_error If @p encoded is not what this codec
	 * makes, or decodes into more than @p limit bytes. The message names the
	 * codec but not the file, which the caller adds.
	 */
	virtual void decode(
		span<const byte> encoded,
		std::size_t limit,
		std::vector<byte> &decoded
	) const = 0;
};

} // namespace zarr
} // namespace vitrio
