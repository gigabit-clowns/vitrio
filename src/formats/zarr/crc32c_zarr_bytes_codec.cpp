// SPDX-License-Identifier: LGPL-2.1-or-later

#include "crc32c_zarr_bytes_codec.hpp"

#include "crc32c.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <cstdint>

namespace vitrio
{
namespace zarr
{

namespace
{

const std::size_t checksum_size = 4;

} // anonymous namespace

void crc32c_zarr_bytes_codec::decode(
	span<const byte> encoded,
	std::size_t limit,
	std::vector<byte> &decoded
) const
{
	if (encoded.size() < checksum_size)
	{
		throw image_file_format_error(
			"crc32c_zarr_bytes_codec: The chunk is too short to end in a "
			"checksum."
		);
	}

	const auto size = encoded.size() - checksum_size;
	if (size > limit)
	{
		throw image_file_format_error(
			"crc32c_zarr_bytes_codec: The chunk decodes into more bytes than "
			"it may hold."
		);
	}

	const auto *const stored = encoded.data() + size;
	const auto expected =
		static_cast<std::uint32_t>(stored[0]) |
		static_cast<std::uint32_t>(stored[1]) << 8 |
		static_cast<std::uint32_t>(stored[2]) << 16 |
		static_cast<std::uint32_t>(stored[3]) << 24;
	if (compute_crc32c(make_span(encoded.data(), size)) != expected)
	{
		throw image_file_format_error(
			"crc32c_zarr_bytes_codec: The checksum of the chunk does not "
			"match its bytes."
		);
	}

	decoded.assign(encoded.data(), encoded.data() + size);
}

void crc32c_zarr_bytes_codec::encode(
	span<const byte> decoded,
	std::vector<byte> &encoded
) const
{
	const auto checksum = compute_crc32c(decoded);
	encoded.assign(decoded.begin(), decoded.end());
	for (std::size_t i = 0; i < checksum_size; ++i)
	{
		encoded.push_back(static_cast<byte>(checksum >> (8 * i)));
	}
}

} // namespace zarr
} // namespace vitrio
