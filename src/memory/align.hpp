// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>
#include <cstdint>

namespace vitrio
{

/**
 * @brief Check whether an address is a multiple of an alignment.
 *
 * @param address The address.
 * @param alignment The alignment. Must not be zero.
 * @return bool true if @p address is a multiple of @p alignment.
 */
constexpr bool
is_aligned(std::uintptr_t address, std::size_t alignment) noexcept;

/**
 * @brief Check whether a pointer is aligned to a boundary.
 *
 * @tparam T Type pointed at.
 * @param address The pointer.
 * @param alignment The alignment. Must not be zero.
 * @return bool true if the address of @p address is a multiple of
 * @p alignment.
 */
template <typename T>
bool is_aligned(T *address, std::size_t alignment) noexcept;

/**
 * @brief Round an address up to a multiple of an alignment.
 *
 * @param address The address.
 * @param alignment The alignment. Must not be zero.
 * @return std::uintptr_t The smallest multiple of @p alignment that is not
 * below @p address.
 */
constexpr std::uintptr_t
align_ceil(std::uintptr_t address, std::size_t alignment) noexcept;

} // namespace vitrio

#include "align.inl"
