// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_bytes_codec.hpp"

#include <string>

namespace vitrio
{
namespace zarr
{

/**
 * @brief The blosc codec, which c-blosc decodes. Each chunk carries a header of
 * its own, which says how it was shuffled, with which compressor and into how
 * many bytes, so decoding asks for no configuration.
 */
class blosc_zarr_bytes_codec final
	: public zarr_bytes_codec
{
public:
	/**
	 * @brief Construct the codec, with how it compresses what it encodes.
	 *
	 * Decoding needs none of it: a chunk of blosc says how it was encoded.
	 *
	 * @param compressor The compressor blosc compresses with, as blosc names
	 * it: "lz4", "zstd", "blosclz", "lz4hc" or "zlib".
	 * @param level How hard it compresses, from 0 to 9.
	 * @param shuffle How the bytes are shuffled first: 0 not at all, 1 a byte
	 * at a time, 2 a bit at a time.
	 * @param type_size The size of a value, which shuffling goes by.
	 */
	explicit blosc_zarr_bytes_codec(
		std::string compressor = "zstd",
		int level = 5,
		int shuffle = 1,
		std::size_t type_size = 1
	);
	~blosc_zarr_bytes_codec() override = default;

	void decode(
		span<const byte> encoded,
		std::size_t limit,
		std::vector<byte> &decoded
	) const override;

	void encode(
		span<const byte> decoded,
		std::vector<byte> &encoded
	) const override;

private:
	std::string m_compressor;
	int m_level;
	int m_shuffle;
	std::size_t m_type_size;
};

} // namespace zarr
} // namespace vitrio
