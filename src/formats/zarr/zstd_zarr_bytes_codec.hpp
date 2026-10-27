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
	/**
	 * @brief Construct the codec, with how hard it compresses.
	 *
	 * @param level The level of zstd it compresses at.
	 */
	explicit zstd_zarr_bytes_codec(int level = 3) noexcept;
	~zstd_zarr_bytes_codec() override = default;

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
