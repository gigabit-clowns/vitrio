// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_bytes_codec.hpp"

namespace vitrio
{
namespace zarr
{

/**
 * @brief The zlib codec of ZARR version 2, a zlib stream: deflate behind a
 * header of two bytes and ahead of an Adler-32 checksum.
 */
class zlib_zarr_bytes_codec final
	: public zarr_bytes_codec
{
public:
	/**
	 * @brief Construct the codec, with how hard it compresses.
	 *
	 * @param level The level of zlib it compresses at, from 0 to 9.
	 */
	explicit zlib_zarr_bytes_codec(int level = 6) noexcept;
	~zlib_zarr_bytes_codec() override = default;

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
	int m_level;
};

} // namespace zarr
} // namespace vitrio
