// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "align.hpp"

namespace vitrio
{

constexpr bool
is_aligned(std::uintptr_t address, std::size_t alignment) noexcept
{
	return address % alignment == 0;
}

template <typename T>
inline bool is_aligned(T *address, std::size_t alignment) noexcept
{
	return is_aligned(reinterpret_cast<std::uintptr_t>(address), alignment);
}

constexpr std::uintptr_t
align_ceil(std::uintptr_t address, std::size_t alignment) noexcept
{
	return (address + alignment - 1) / alignment * alignment;
}

} // namespace vitrio
