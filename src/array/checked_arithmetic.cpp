// SPDX-License-Identifier: LGPL-2.1-or-later

#include "checked_arithmetic.hpp"

#include <limits>

namespace vitrio
{

namespace
{

constexpr auto lowest = std::numeric_limits<std::ptrdiff_t>::min();
constexpr auto highest = std::numeric_limits<std::ptrdiff_t>::max();

} // anonymous namespace

bool checked_add(
	std::ptrdiff_t lhs,
	std::ptrdiff_t rhs,
	std::ptrdiff_t &result
) noexcept
{
	if (rhs > 0 && lhs > highest - rhs)
	{
		return false;
	}
	if (rhs < 0 && lhs < lowest - rhs)
	{
		return false;
	}

	result = lhs + rhs;
	return true;
}

bool checked_multiply(
	std::ptrdiff_t lhs,
	std::ptrdiff_t rhs,
	std::ptrdiff_t &result
) noexcept
{
	if (lhs == 0 || rhs == 0)
	{
		result = 0;
		return true;
	}

	// Each branch divides a limit by a factor of the sign that keeps the
	// division itself from overflowing, which the lowest value over minus
	// one would.
	if (lhs > 0)
	{
		if (rhs > 0 ? lhs > highest / rhs : rhs < lowest / lhs)
		{
			return false;
		}
	}
	else
	{
		if (rhs > 0 ? lhs < lowest / rhs : lhs < highest / rhs)
		{
			return false;
		}
	}

	result = lhs * rhs;
	return true;
}

} // namespace vitrio
