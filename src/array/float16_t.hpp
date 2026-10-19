// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>

namespace vitrio
{

/**
 * @brief A floating point number of half precision, laid out as IEEE 754
 * lays out binary16.
 *
 * It does no arithmetic of its own: it is what a file holds, converted to
 * a float to be computed with and from a float or a double to be stored.
 * Both conversions are bit arithmetic with no branch, so that a loop of
 * them is one a compiler vectorizes.
 */
class float16_t
{
public:
	/**
	 * @brief Construct a number whose value is indeterminate, as that of a
	 * float is.
	 *
	 * A value-initialized one, such as @c float16_t(), is positive zero.
	 */
	float16_t() = default;

	/**
	 * @brief Construct the half precision number nearest to a float.
	 *
	 * It rounds to nearest, ties to even, as IEEE 754 does by default. A
	 * value of 65520 or more becomes an infinity, and a NaN stays a NaN of
	 * the same sign, quieted, with as much of its payload as fits.
	 *
	 * @param value The float.
	 */
	explicit float16_t(float value) noexcept;

	/**
	 * @brief Construct the half precision number nearest to a double.
	 *
	 * The double is rounded once, as a float is. Going through a float
	 * instead would round twice, which is not the same: a value a little
	 * past the midpoint of two half precision numbers can become that
	 * midpoint as a float, and then round to the wrong one.
	 *
	 * @param value The double.
	 */
	explicit float16_t(double value) noexcept;

	/**
	 * @brief Get the value as a float.
	 *
	 * Every half precision number is a float, so nothing is rounded, and a
	 * NaN keeps its payload.
	 *
	 * @return float The value.
	 */
	explicit operator float() const noexcept;

	/**
	 * @brief Construct the number a pattern of bits encodes.
	 *
	 * @param bits The sign, exponent and mantissa, as binary16 lays them
	 * out.
	 * @return float16_t The number.
	 */
	static float16_t from_bits(std::uint16_t bits) noexcept;

	/**
	 * @brief Get the pattern of bits that encodes the number.
	 *
	 * @return std::uint16_t The sign, exponent and mantissa, as binary16
	 * lays them out.
	 */
	std::uint16_t get_bits() const noexcept;

private:
	std::uint16_t m_bits;
};

} // namespace vitrio

#include "float16_t.inl"
