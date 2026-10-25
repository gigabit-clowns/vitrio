// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gzip_zarr_bytes_codec.hpp"

#include "zlib_inflate.hpp"

namespace vitrio
{
namespace zarr
{

void gzip_zarr_bytes_codec::decode(
	span<const byte> encoded,
	std::size_t limit,
	std::vector<byte> &decoded
) const
{
	inflate_stream(encoded, 31, limit, decoded, "gzip_zarr_bytes_codec");
}

} // namespace zarr
} // namespace vitrio
