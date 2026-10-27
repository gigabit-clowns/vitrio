// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstdint>

namespace vitrio
{
namespace zarr
{

/**
 * @brief Compute the CRC-32C of some bytes, the checksum of Castagnoli that
 * the crc32c codec of ZARR appends to a chunk.
 *
 * It is the reflected CRC of the polynomial 0x1EDC6F41, started at all ones
 * and inverted at the end, as iSCSI and ext4 have it: the CRC-32C of the
 * nine bytes "123456789" is 0xE3069283.
 *
 * @param bytes The bytes.
 * @return std::uint32_t The checksum.
 */
std::uint32_t compute_crc32c(span<const byte> bytes) noexcept;

} // namespace zarr
} // namespace vitrio
