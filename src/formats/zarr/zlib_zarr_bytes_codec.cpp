// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zlib_zarr_bytes_codec.hpp"

#include <compression/zlib_deflate.hpp>
#include <compression/zlib_inflate.hpp>

namespace vitrio
{
namespace zarr
{

zlib_zarr_bytes_codec::zlib_zarr_bytes_codec(int level) noexcept
	: m_level(level)
{
}

void zlib_zarr_bytes_codec::decode(
	span<const byte> encoded,
	std::size_t limit,
	std::vector<byte> &decoded
) const
{
	inflate_stream(encoded, 15, limit, decoded, "zlib_zarr_bytes_codec");
}

void zlib_zarr_bytes_codec::encode(
	span<const byte> decoded,
	std::vector<byte> &encoded
) const
{
	deflate_stream(decoded, 15, m_level, encoded, "zlib_zarr_bytes_codec");
}

} // namespace zarr
} // namespace vitrio
