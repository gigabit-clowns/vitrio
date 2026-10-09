// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed_width_float.hpp"

#include <vitrio/array/numerical_type.hpp>

#include <complex>
#include <cstdint>
#include <type_traits>

namespace vitrio
{

/**
 * @brief Get the numerical_type a C++ type stands for.
 *
 * Its @c value is the numerical_type. It is defined only for the types the
 * elements of an array may have.
 *
 * @tparam T The C++ type.
 */
template <typename T>
struct numerical_type_of;

template <>
struct numerical_type_of<bool>
	: std::integral_constant<numerical_type, numerical_type::boolean>
{
};

template <>
struct numerical_type_of<std::int8_t>
	: std::integral_constant<numerical_type, numerical_type::int8>
{
};

template <>
struct numerical_type_of<std::uint8_t>
	: std::integral_constant<numerical_type, numerical_type::uint8>
{
};

template <>
struct numerical_type_of<std::int16_t>
	: std::integral_constant<numerical_type, numerical_type::int16>
{
};

template <>
struct numerical_type_of<std::uint16_t>
	: std::integral_constant<numerical_type, numerical_type::uint16>
{
};

template <>
struct numerical_type_of<std::int32_t>
	: std::integral_constant<numerical_type, numerical_type::int32>
{
};

template <>
struct numerical_type_of<std::uint32_t>
	: std::integral_constant<numerical_type, numerical_type::uint32>
{
};

template <>
struct numerical_type_of<std::int64_t>
	: std::integral_constant<numerical_type, numerical_type::int64>
{
};

template <>
struct numerical_type_of<std::uint64_t>
	: std::integral_constant<numerical_type, numerical_type::uint64>
{
};

template <>
struct numerical_type_of<float16_t>
	: std::integral_constant<numerical_type, numerical_type::float16>
{
};

template <>
struct numerical_type_of<float32_t>
	: std::integral_constant<numerical_type, numerical_type::float32>
{
};

template <>
struct numerical_type_of<float64_t>
	: std::integral_constant<numerical_type, numerical_type::float64>
{
};

template <>
struct numerical_type_of<std::complex<float16_t>>
	: std::integral_constant<numerical_type, numerical_type::complex_float16>
{
};

template <>
struct numerical_type_of<std::complex<float32_t>>
	: std::integral_constant<numerical_type, numerical_type::complex_float32>
{
};

template <>
struct numerical_type_of<std::complex<float64_t>>
	: std::integral_constant<numerical_type, numerical_type::complex_float64>
{
};

} // namespace vitrio
