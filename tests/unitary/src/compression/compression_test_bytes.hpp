// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

// Bytes a compressor makes something of, neither all alike nor random: a
// ramp that repeats, as the values of an image often nearly do.
inline std::vector<byte> make_codec_test_bytes(std::size_t size)
{
	std::vector<byte> bytes(size);
	for (std::size_t i = 0; i < size; ++i)
	{
		bytes[i] = static_cast<byte>((i * 7 + i / 251) % 256);
	}

	return bytes;
}

inline span<const byte> as_span(const std::vector<byte> &bytes)
{
	return make_span(bytes.data(), bytes.size());
}

} // namespace vitrio
