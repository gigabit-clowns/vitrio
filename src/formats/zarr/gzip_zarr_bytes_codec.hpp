// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_bytes_codec.hpp"

namespace vitrio
{
namespace zarr
{

/**
 * @brief The gzip codec, a gzip stream of a single member: deflate behind a
 * header that may name a file and ahead of a CRC-32 and the length.
 */
class gzip_zarr_bytes_codec final
	: public zarr_bytes_codec
{
public:
	gzip_zarr_bytes_codec() noexcept = default;
	~gzip_zarr_bytes_codec() override = default;

	void decode(
		span<const byte> encoded,
		std::size_t limit,
		std::vector<byte> &decoded
	) const override;
};

} // namespace zarr
} // namespace vitrio
