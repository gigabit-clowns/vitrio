// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "byte_order.hpp"

#include <algorithm>
#include <cstdint>

namespace vitrio
{

constexpr byte_order get_system_byte_order() noexcept
{
	#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		return byte_order::big_endian;
	#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return byte_order::little_endian;
	#elif defined(_WIN32)
		return byte_order::little_endian;
	#else
		#error "The byte order of this platform is unknown."
	#endif
}

// One overload per width, chosen by tag, so that each is the shifts a
// compiler recognizes as a single instruction.
constexpr std::uint8_t
reverse_bytes(std::uint8_t x, std::integral_constant<std::size_t, 1>) noexcept
{
	return x;
}

constexpr std::uint16_t reverse_bytes(
	std::uint16_t x,
	std::integral_constant<std::size_t, 2>
) noexcept
{
	return static_cast<std::uint16_t>((x << 8) | (x >> 8));
}

constexpr std::uint32_t reverse_bytes(
	std::uint32_t x,
	std::integral_constant<std::size_t, 4>
) noexcept
{
	return
		(x >> 24) |
		((x >> 8) & 0x0000FF00U) |
		((x << 8) & 0x00FF0000U) |
		(x << 24);
}

constexpr std::uint64_t reverse_bytes(
	std::uint64_t x,
	std::integral_constant<std::size_t, 8>
) noexcept
{
	return
		(x >> 56) |
		((x >> 40) & 0x000000000000FF00ULL) |
		((x >> 24) & 0x0000000000FF0000ULL) |
		((x >> 8) & 0x00000000FF000000ULL) |
		((x << 8) & 0x000000FF00000000ULL) |
		((x << 24) & 0x0000FF0000000000ULL) |
		((x << 40) & 0x00FF000000000000ULL) |
		(x << 56);
}

// A value that is not an integer is reversed through its bytes in memory.
template <typename T>
inline T reverse_object_bytes(T x) noexcept
{
	auto *bytes = reinterpret_cast<unsigned char*>(&x);
	std::reverse(bytes, bytes + sizeof(T));
	return x;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, T>::type
reverse_byte_order(T x) noexcept
{
	using unsigned_type = typename std::make_unsigned<T>::type;
	using width_type = std::integral_constant<std::size_t, sizeof(T)>;

	return static_cast<T>(
		reverse_bytes(static_cast<unsigned_type>(x), width_type())
	);
}

inline float reverse_byte_order(float x) noexcept
{
	return reverse_object_bytes(x);
}

inline double reverse_byte_order(double x) noexcept
{
	return reverse_object_bytes(x);
}

inline float16_t reverse_byte_order(float16_t x) noexcept
{
	return reverse_object_bytes(x);
}

template <typename T>
inline std::complex<T> reverse_byte_order(const std::complex<T> &x) noexcept
{
	return std::complex<T>(
		reverse_byte_order(x.real()),
		reverse_byte_order(x.imag())
	);
}

} // namespace vitrio
