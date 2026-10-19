// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace vitrio
{

/**
 * @brief Get the value whose bytes are those of another of the same size.
 *
 * It is what std::bit_cast of C++20 does, through std::memcpy, which a
 * compiler turns into a move between registers, or into nothing.
 *
 * @tparam To Type of the value returned. Trivially copyable.
 * @tparam From Type of the value given. Trivially copyable, and of the
 * size of @p To.
 * @param from The value.
 * @return To The value whose bytes are those of @p from.
 */
template <typename To, typename From>
To reinterpret_bits(const From &from) noexcept;

} // namespace vitrio

#include "reinterpret_bits.inl"
