// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_bytes_codec.hpp"

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
	blosc_zarr_bytes_codec() noexcept = default;
	~blosc_zarr_bytes_codec() override = default;

	void decode(
		span<const byte> encoded,
		std::size_t limit,
		std::vector<byte> &decoded
	) const override;
};

} // namespace zarr
} // namespace vitrio
