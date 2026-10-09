// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <array/fixed_width_float.hpp>

#include <complex>
#include <type_traits>

namespace vitrio
{

/**
 * @brief The order the bytes of a number are stored in.
 */
enum class byte_order
{
	big_endian,
	little_endian,
};

/**
 * @brief Get the byte order of the machine the program runs on.
 *
 * @return byte_order The byte order.
 */
constexpr byte_order get_system_byte_order() noexcept;

/**
 * @brief Reverse the bytes of an integer.
 *
 * @tparam T Type of the integer.
 * @param x The integer.
 * @return T The integer whose bytes are those of @p x in the opposite order.
 */
template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, T>::type
reverse_byte_order(T x) noexcept;

/**
 * @brief Reverse the bytes of a float.
 *
 * The bytes are reversed as they are stored, so the result is in general
 * not a meaningful number until it is reversed again.
 *
 * @param x The float.
 * @return float The value whose bytes are those of @p x in the opposite
 * order.
 */
float reverse_byte_order(float x) noexcept;

/**
 * @brief Reverse the bytes of a double.
 *
 * @param x The double.
 * @return double The value whose bytes are those of @p x in the opposite
 * order.
 *
 * @see reverse_byte_order(float)
 */
double reverse_byte_order(double x) noexcept;

/**
 * @brief Reverse the bytes of a half precision number.
 *
 * @param x The number.
 * @return float16_t The value whose bytes are those of @p x in the opposite
 * order.
 *
 * @see reverse_byte_order(float)
 */
float16_t reverse_byte_order(float16_t x) noexcept;

/**
 * @brief Reverse the bytes of each component of a complex number.
 *
 * The real part stays first: only the bytes within each of the two
 * components are reversed.
 *
 * @tparam T Type of the components.
 * @param x The complex number.
 * @return std::complex<T> The value with both components reversed.
 */
template <typename T>
std::complex<T> reverse_byte_order(const std::complex<T> &x) noexcept;

/**
 * @brief Convert an integer from one byte order to another.
 *
 * @tparam T Type of the integer.
 * @param x The integer, in the byte order @p from.
 * @param from The byte order @p x is in.
 * @param to The byte order to convert it to.
 * @return T @p x when the two byte orders are the same, and @p x with its
 * bytes reversed otherwise.
 */
template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, T>::type
convert_byte_order(T x, byte_order from, byte_order to) noexcept;

} // namespace vitrio

#include "byte_order.inl"
