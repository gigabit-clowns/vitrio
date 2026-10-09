// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>

namespace vitrio
{

/**
 * @brief Get the size of a page of memory on this machine.
 *
 * @return std::size_t The size in bytes. Never zero.
 */
std::size_t get_page_size() noexcept;

} // namespace vitrio
