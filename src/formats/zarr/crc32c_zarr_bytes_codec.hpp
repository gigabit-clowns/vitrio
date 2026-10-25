// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_bytes_codec.hpp"

namespace vitrio
{
namespace zarr
{

/**
 * @brief The crc32c codec of ZARR version 3, which appends the CRC-32C of the
 * bytes, little endian. Decoding checks it and strips it.
 */
class crc32c_zarr_bytes_codec final
	: public zarr_bytes_codec
{
public:
	crc32c_zarr_bytes_codec() noexcept = default;
	~crc32c_zarr_bytes_codec() override = default;

	void decode(
		span<const byte> encoded,
		std::size_t limit,
		std::vector<byte> &decoded
	) const override;
};

} // namespace zarr
} // namespace vitrio
