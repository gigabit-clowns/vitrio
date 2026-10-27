// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>

#include <cstddef>
#include <memory>

namespace vitrio
{

/**
 * @brief Allocate memory straight from the system, which goes back to it as
 * soon as the last owner lets it go.
 *
 * The allocator of the process keeps what is freed for later, in an arena
 * per thread: memory that threads allocate and free over and over, a buffer
 * at a time, stays the process's at the most any arena once held. This
 * memory does not, at the cost of the system zeroing its pages anew.
 *
 * @param size The bytes. Not zero.
 * @return std::shared_ptr<byte> The memory, aligned at a page and filled
 * with zeros.
 * @throws std::bad_alloc If the system has none to give.
 */
std::shared_ptr<byte> allocate_system_memory(std::size_t size);

} // namespace vitrio
