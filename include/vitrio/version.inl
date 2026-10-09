// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "version.hpp"

namespace vitrio
{

constexpr version::version(
	std::uint32_t major,
	std::uint32_t minor,
	std::uint32_t patch
) noexcept
	: m_major(major)
	, m_minor(minor)
	, m_patch(patch)
{
}

constexpr std::uint32_t version::get_major() const noexcept
{
	return m_major;
}

constexpr std::uint32_t version::get_minor() const noexcept
{
	return m_minor;
}

constexpr std::uint32_t version::get_patch() const noexcept
{
	return m_patch;
}

} // namespace vitrio
