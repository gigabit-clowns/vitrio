// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "reinterpret_bits.hpp"

#include <cstring>
#include <type_traits>

namespace vitrio
{

template <typename To, typename From>
inline To reinterpret_bits(const From &from) noexcept
{
	static_assert(sizeof(To) == sizeof(From), "The sizes should match");
	static_assert(
		std::is_trivially_copyable<To>::value &&
		std::is_trivially_copyable<From>::value,
		"Both types should be trivially copyable"
	);

	To to;
	std::memcpy(&to, &from, sizeof(to));
	return to;
}

} // namespace vitrio
