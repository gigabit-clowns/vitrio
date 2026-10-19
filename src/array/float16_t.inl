// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "float16_t.hpp"

#include <memory/reinterpret_bits.hpp>

#include <cstdint>

namespace vitrio
{

// One of two values, chosen with a mask rather than a branch. GCC does not
// move a floating point operation out of a branch by default, as it may
// raise an exception, and a loop with a branch left in it is not
// vectorized. So the conversions below compute every case and choose
// among them with this.
template <typename Bits>
inline Bits select_bits(bool condition, Bits if_true, Bits if_false) noexcept
{
	const auto mask = static_cast<Bits>(Bits(0) - Bits(condition));
	return (if_true & mask) | (if_false & ~mask);
}

// What narrowing needs to know of the format it narrows from.
template <typename Float>
struct float16_source_format;

template <>
struct float16_source_format<float>
{
	using bits_type = std::uint32_t;
	static constexpr bits_type mantissa_width = 23;
	static constexpr bits_type exponent_bias = 127;
};

template <>
struct float16_source_format<double>
{
	using bits_type = std::uint64_t;
	static constexpr bits_type mantissa_width = 52;
	static constexpr bits_type exponent_bias = 1023;
};

// Rounds to nearest, ties to even, as Fabian Giesen's float_to_half_fast3_rtne
// does, for a float or a double alike.
template <typename Float>
inline std::uint16_t round_to_float16_bits(Float value) noexcept
{
	using format = float16_source_format<Float>;
	using bits_type = typename format::bits_type;
	constexpr bits_type width = sizeof(bits_type) * 8;
	constexpr bits_type mantissa_width = format::mantissa_width;
	constexpr bits_type bias = format::exponent_bias;
	// How far the mantissa moves to keep the ten bits of a half.
	constexpr bits_type shift = mantissa_width - 10;
	constexpr bits_type infinity =
		((bits_type(1) << (width - mantissa_width - 1)) - 1) << mantissa_width;

	const auto bits = reinterpret_bits<bits_type>(value);
	const bits_type sign = bits & (bits_type(1) << (width - 1));
	const bits_type magnitude = bits ^ sign;

	// 65536 and beyond is an infinity, and a NaN stays one, with the top
	// of its payload. Its quiet bit is set, which also keeps a NaN whose
	// payload lies only in the bits shifted out from becoming an infinity.
	const bits_type special = 0x7C00U | select_bits(
		magnitude > infinity,
		bits_type(0x200U | ((magnitude >> shift) & 0x3FFU)),
		bits_type(0)
	);

	// A normal half has its exponent rebiased. Adding half of what is
	// shifted out, less one, plus the lowest bit kept, rounds to nearest
	// with ties to even. A mantissa that carries over raises the exponent,
	// to that of an infinity from 65520 on.
	const bits_type normal = (
		magnitude - ((bias - 15) << mantissa_width) +
		((bits_type(1) << (shift - 1)) - 1) +
		((magnitude >> shift) & 1U)
	) >> shift;

	// A subnormal half, or zero, is rounded by the floating point addition
	// itself: added to a number whose ulp is 2^-24, that of a subnormal
	// half, the value is rounded to a multiple of it, and the bits of that
	// number then subtracted leave the half.
	constexpr bits_type magic = (bias + mantissa_width - 24) << mantissa_width;
	const bits_type subnormal = reinterpret_bits<bits_type>(
		reinterpret_bits<Float>(magnitude) + reinterpret_bits<Float>(magic)
	) - magic;

	const bits_type finite = select_bits(
		magnitude < ((bias - 14) << mantissa_width),
		subnormal,
		normal
	);
	const bits_type result = select_bits(
		magnitude >= ((bias + 16) << mantissa_width),
		special,
		finite
	);
	return static_cast<std::uint16_t>(result | (sign >> (width - 16)));
}

inline float16_t::float16_t(float value) noexcept
	: m_bits(round_to_float16_bits(value))
{
}

inline float16_t::float16_t(double value) noexcept
	: m_bits(round_to_float16_bits(value))
{
}

inline float16_t::operator float() const noexcept
{
	const std::uint32_t bits = m_bits;
	const std::uint32_t sign = (bits & 0x8000U) << 16;
	// The exponent and the mantissa, where a float holds them.
	const std::uint32_t magnitude = (bits & 0x7FFFU) << 13;
	const std::uint32_t exponent = magnitude & (0x7C00U << 13);

	// A normal half has its exponent rebiased. An infinity or a NaN, whose
	// exponent is all ones, is moved further, to the exponent of all ones of
	// a float, with its mantissa and so with the payload of a NaN.
	const std::uint32_t normal =
		magnitude + ((127U - 15U) << 23) +
		select_bits(exponent == (0x7C00U << 13), (128U - 16U) << 23, 0U);

	// A subnormal half, or zero, given the exponent of 2^-14, is the number
	// plus 2^-14, which one exact subtraction takes away.
	const float subnormal =
		reinterpret_bits<float>(magnitude + ((127U - 14U) << 23)) -
		reinterpret_bits<float>((127U - 14U) << 23);

	const std::uint32_t result = select_bits(
		exponent == 0,
		reinterpret_bits<std::uint32_t>(subnormal),
		normal
	);
	return reinterpret_bits<float>(result | sign);
}

inline float16_t float16_t::from_bits(std::uint16_t bits) noexcept
{
	float16_t result;
	result.m_bits = bits;
	return result;
}

inline std::uint16_t float16_t::get_bits() const noexcept
{
	return m_bits;
}

} // namespace vitrio
