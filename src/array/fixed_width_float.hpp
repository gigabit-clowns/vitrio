// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <half.hpp>

namespace vitrio
{

/**
 * @brief A floating point number of half precision.
 */
using float16_t = half_float::half;
static_assert(sizeof(float16_t) == 2, "float16_t should be 2 bytes long");

/**
 * @brief A floating point number of single precision.
 */
using float32_t = float;
static_assert(sizeof(float32_t) == 4, "float32_t should be 4 bytes long");

/**
 * @brief A floating point number of double precision.
 */
using float64_t = double;
static_assert(sizeof(float64_t) == 8, "float64_t should be 8 bytes long");

} // namespace vitrio
