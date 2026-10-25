// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_bytes_codec.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief The codecs that turn bytes into bytes which a ZARR array encodes its
 * chunks with, in the order they were applied.
 *
 * A chunk is decoded through them in the reverse order. A pipeline without
 * codecs stores the bytes of a chunk as they are, which the reader of a
 * chunk then reads without copying them.
 */
class zarr_codec_pipeline
{
public:
	/**
	 * @brief Construct a pipeline of no codecs.
	 */
	zarr_codec_pipeline() noexcept;

	/**
	 * @brief Construct a pipeline of codecs.
	 *
	 * @param codecs The codecs, in the order they encode. None may be null.
	 * @throws std::invalid_argument If a codec is null.
	 */
	explicit zarr_codec_pipeline(
		std::vector<std::shared_ptr<const zarr_bytes_codec>> codecs
	);

	zarr_codec_pipeline(const zarr_codec_pipeline &other);
	zarr_codec_pipeline(zarr_codec_pipeline &&other) noexcept;
	~zarr_codec_pipeline();

	zarr_codec_pipeline& operator=(const zarr_codec_pipeline &other);
	zarr_codec_pipeline& operator=(zarr_codec_pipeline &&other) noexcept;

	/**
	 * @brief Check whether the pipeline stores bytes as they are.
	 *
	 * @return true It holds no codec.
	 * @return false It holds one at least.
	 */
	bool is_empty() const noexcept;

	/**
	 * @brief Get how many codecs the pipeline holds.
	 *
	 * @return std::size_t The number of codecs.
	 */
	std::size_t get_codec_count() const noexcept;

	/**
	 * @brief Decode the bytes of a chunk through every codec, in reverse.
	 *
	 * Each codec but the last to decode may produce as many bytes as it is
	 * given or @p size, whichever is more, which allows for one that adds
	 * bytes, such as a checksum, ahead of another. The last must produce
	 * exactly @p size bytes.
	 *
	 * @param encoded The bytes of the chunk, as they are stored.
	 * @param size How many bytes the chunk decodes into.
	 * @param decoded Where they are written, replacing what it held.
	 * @throws image_file_format_error If a codec refuses its bytes, or the
	 * last does not produce @p size bytes.
	 */
	void decode(
		span<const byte> encoded,
		std::size_t size,
		std::vector<byte> &decoded
	) const;

	/**
	 * @brief Encode the bytes of a chunk through every codec, in order.
	 *
	 * @param decoded The bytes of the chunk, its values.
	 * @param encoded Where they are written as they are to be stored,
	 * replacing what it held.
	 * @throws std::runtime_error If a codec fails.
	 */
	void encode(span<const byte> decoded, std::vector<byte> &encoded) const;

private:
	std::vector<std::shared_ptr<const zarr_bytes_codec>> m_codecs;
};

} // namespace zarr
} // namespace vitrio
