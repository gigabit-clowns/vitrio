// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tiff_signature.hpp"

#include <cstddef>
#include <cstdint>

namespace vitrio
{
namespace tiff
{

namespace
{

const std::size_t signature_size = 4;
const std::uint8_t little_endian_mark = 0x49;
const std::uint8_t big_endian_mark = 0x4D;
const std::uint8_t classic_version = 42;
const std::uint8_t big_version = 43;

bool is_version(std::uint8_t low, std::uint8_t high) noexcept
{
	return high == 0 && (low == classic_version || low == big_version);
}

} // anonymous namespace

bool has_signature(span<const byte> leading_bytes) noexcept
{
	if (leading_bytes.size() < signature_size)
	{
		return false;
	}

	const auto mark = leading_bytes[0];
	if (leading_bytes[1] != mark)
	{
		return false;
	}

	const auto third = leading_bytes[2];
	const auto fourth = leading_bytes[3];

	if (mark == little_endian_mark)
	{
		return is_version(third, fourth);
	}

	if (mark == big_endian_mark)
	{
		return is_version(fourth, third);
	}

	return false;
}

} // namespace tiff
} // namespace vitrio
