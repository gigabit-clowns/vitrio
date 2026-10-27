// SPDX-License-Identifier: LGPL-2.1-or-later

#include "crc32c.hpp"

#include <array>

namespace vitrio
{
namespace zarr
{

namespace
{

// The polynomial of Castagnoli, its bits reversed.
const std::uint32_t reflected_polynomial = 0x82F63B78U;

// The remainder of each byte, which the checksum takes a byte at a time.
std::array<std::uint32_t, 256> make_table() noexcept
{
	std::array<std::uint32_t, 256> table;
	for (std::uint32_t value = 0; value < table.size(); ++value)
	{
		auto remainder = value;
		for (int bit = 0; bit < 8; ++bit)
		{
			remainder = (remainder >> 1) ^
				((remainder & 1U) != 0 ? reflected_polynomial : 0U);
		}
		table[value] = remainder;
	}

	return table;
}

} // anonymous namespace

std::uint32_t compute_crc32c(span<const byte> bytes) noexcept
{
	static const auto table = make_table();

	auto remainder = 0xFFFFFFFFU;
	for (const auto value : bytes)
	{
		remainder = table[(remainder ^ value) & 0xFFU] ^ (remainder >> 8);
	}

	return remainder ^ 0xFFFFFFFFU;
}

} // namespace zarr
} // namespace vitrio
