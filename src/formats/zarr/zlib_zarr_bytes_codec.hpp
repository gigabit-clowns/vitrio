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
	zlib_zarr_bytes_codec() noexcept = default;
	~zlib_zarr_bytes_codec() override = default;

	void decode(
		span<const byte> encoded,
		std::size_t limit,
		std::vector<byte> &decoded
	) const override;
};

} // namespace zarr
} // namespace vitrio
