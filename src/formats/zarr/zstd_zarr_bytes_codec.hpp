// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_bytes_codec.hpp"

namespace vitrio
{
namespace zarr
{

/**
 * @brief The zstd codec, a zstd frame or several, one after another. A frame
 * need not state how many bytes it holds, in which case the bytes are decoded
 * as they come, up to the limit.
 */
class zstd_zarr_bytes_codec final
	: public zarr_bytes_codec
{
public:
	zstd_zarr_bytes_codec() noexcept = default;
	~zstd_zarr_bytes_codec() override = default;

	void decode(
		span<const byte> encoded,
		std::size_t limit,
		std::vector<byte> &decoded
	) const override;
};

} // namespace zarr
} // namespace vitrio
