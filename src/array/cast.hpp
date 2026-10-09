// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed_width_float.hpp"

#include <complex>
#include <type_traits>

namespace vitrio
{

/**
 * @brief Convert one element into another type.
 *
 * The value is converted as static_cast converts between the two types.
 * These functions are inline on purpose: they run once per element, and a
 * call that is not inlined keeps a loop from being vectorized.
 *
 * @tparam T Type converted into.
 * @tparam Q Type converted from.
 * @param destination Where the converted value is written.
 * @param source The value to convert.
 */
template <typename T, typename Q>
void cast(T *destination, const Q *source) noexcept;

/**
 * @brief Convert one element into half precision.
 *
 * The value goes through a float and is rounded to the nearest half
 * precision number.
 *
 * @tparam Q Type converted from.
 * @param destination Where the converted value is written.
 * @param source The value to convert.
 */
template <typename Q>
typename std::enable_if<std::is_arithmetic<Q>::value>::type
cast(float16_t *destination, const Q *source) noexcept;

/**
 * @brief Convert one half precision element into another type.
 *
 * @tparam T Type converted into.
 * @param destination Where the converted value is written.
 * @param source The value to convert.
 */
template <typename T>
typename std::enable_if<std::is_arithmetic<T>::value>::type
cast(T *destination, const float16_t *source) noexcept;

/**
 * @brief Convert one complex element into another complex type.
 *
 * Each of the two components is converted on its own.
 *
 * @tparam T Type of the components converted into.
 * @tparam Q Type of the components converted from.
 * @param destination Where the converted value is written.
 * @param source The value to convert.
 */
template <typename T, typename Q>
void cast(std::complex<T> *destination, const std::complex<Q> *source) noexcept;

/**
 * @brief Convert one real element into a complex type.
 *
 * The value becomes the real part, and the imaginary part is zero.
 *
 * @tparam T Type of the components converted into.
 * @tparam Q Type converted from.
 * @param destination Where the converted value is written.
 * @param source The value to convert.
 */
template <typename T, typename Q>
void cast(std::complex<T> *destination, const Q *source) noexcept;

} // namespace vitrio

#include "cast.inl"
