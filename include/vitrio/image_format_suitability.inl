// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_format_suitability.hpp"

#include <type_traits>

namespace vitrio
{

constexpr bool operator<(
	image_format_suitability lhs,
	image_format_suitability rhs
) noexcept
{
	using value_type = std::underlying_type<image_format_suitability>::type;

	return static_cast<value_type>(lhs) < static_cast<value_type>(rhs);
}

constexpr bool operator<=(
	image_format_suitability lhs,
	image_format_suitability rhs
) noexcept
{
	return !(rhs < lhs);
}

constexpr bool operator>(
	image_format_suitability lhs,
	image_format_suitability rhs
) noexcept
{
	return rhs < lhs;
}

constexpr bool operator>=(
	image_format_suitability lhs,
	image_format_suitability rhs
) noexcept
{
	return !(lhs < rhs);
}

constexpr const char* to_string(image_format_suitability suitability) noexcept
{
	switch (suitability)
	{
	case image_format_suitability::unsupported: return "unsupported";
	case image_format_suitability::fallback: return "fallback";
	case image_format_suitability::normal: return "normal";
	case image_format_suitability::optimal: return "optimal";
	default: return "";
	}
}

template <typename T>
inline std::basic_ostream<T>& operator<<(
	std::basic_ostream<T> &os,
	image_format_suitability suitability
)
{
	return os << to_string(suitability);
}

} // namespace vitrio
