// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>

namespace vitrio
{

/**
 * @brief Add two offsets, unless their sum does not fit.
 *
 * Extents and strides may come from a file, so the arithmetic on them cannot
 * be trusted not to overflow.
 *
 * @param lhs The first offset.
 * @param rhs The second offset.
 * @param result Where the sum is written. Left untouched on overflow.
 * @return true The sum was written.
 * @return false The sum does not fit in std::ptrdiff_t.
 */
bool checked_add(
	std::ptrdiff_t lhs,
	std::ptrdiff_t rhs,
	std::ptrdiff_t &result
) noexcept;

/**
 * @brief Multiply two offsets, unless their product does not fit.
 *
 * @param lhs The first offset.
 * @param rhs The second offset.
 * @param result Where the product is written. Left untouched on overflow.
 * @return true The product was written.
 * @return false The product does not fit in std::ptrdiff_t.
 *
 * @see checked_add
 */
bool checked_multiply(
	std::ptrdiff_t lhs,
	std::ptrdiff_t rhs,
	std::ptrdiff_t &result
) noexcept;

} // namespace vitrio
