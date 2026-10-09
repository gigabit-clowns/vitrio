// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>

namespace vitrio
{

/**
 * @brief Allocate memory with a given alignment.
 *
 * @param size Number of bytes to allocate. Must be a multiple of
 * @p alignment.
 * @param alignment Alignment of the memory. Must be a power of two.
 * @return void* The memory. Null if it could not be allocated.
 *
 * @see aligned_free
 */
void* aligned_alloc(std::size_t size, std::size_t alignment) noexcept;

/**
 * @brief Release memory allocated with @ref aligned_alloc.
 *
 * @param ptr The memory to release. Null is ignored.
 */
void aligned_free(void *ptr) noexcept;

} // namespace vitrio
