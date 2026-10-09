// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>
#include <ostream>

namespace vitrio
{

/**
 * @brief A version number of three components.
 *
 * Versions are ordered by their major component, then by their minor
 * component, then by their patch component.
 */
class version
{
public:
	/**
	 * @brief Construct a version from its components.
	 *
	 * @param major The major component.
	 * @param minor The minor component.
	 * @param patch The patch component.
	 */
	constexpr version(
		std::uint32_t major,
		std::uint32_t minor,
		std::uint32_t patch
	) noexcept;
	version(const version &other) = default;
	~version() = default;

	version& operator=(const version &other) = default;

	/**
	 * @brief Get the major component.
	 *
	 * @return std::uint32_t The major component.
	 */
	constexpr std::uint32_t get_major() const noexcept;

	/**
	 * @brief Get the minor component.
	 *
	 * @return std::uint32_t The minor component.
	 */
	constexpr std::uint32_t get_minor() const noexcept;

	/**
	 * @brief Get the patch component.
	 *
	 * @return std::uint32_t The patch component.
	 */
	constexpr std::uint32_t get_patch() const noexcept;

	friend constexpr
	bool operator==(const version &lhs, const version &rhs) noexcept
	{
		return
			lhs.get_major() == rhs.get_major() &&
			lhs.get_minor() == rhs.get_minor() &&
			lhs.get_patch() == rhs.get_patch();
	}

	friend constexpr
	bool operator!=(const version &lhs, const version &rhs) noexcept
	{
		return !(lhs == rhs);
	}

	friend constexpr
	bool operator<(const version &lhs, const version &rhs) noexcept
	{
		if (lhs.get_major() != rhs.get_major())
		{
			return lhs.get_major() < rhs.get_major();
		}
		if (lhs.get_minor() != rhs.get_minor())
		{
			return lhs.get_minor() < rhs.get_minor();
		}
		return lhs.get_patch() < rhs.get_patch();
	}

	friend constexpr
	bool operator<=(const version &lhs, const version &rhs) noexcept
	{
		return !(rhs < lhs);
	}

	friend constexpr
	bool operator>(const version &lhs, const version &rhs) noexcept
	{
		return rhs < lhs;
	}

	friend constexpr
	bool operator>=(const version &lhs, const version &rhs) noexcept
	{
		return !(lhs < rhs);
	}

	template <typename T>
	friend std::basic_ostream<T>&
	operator<<(std::basic_ostream<T> &os, const version &ver)
	{
		return os
			<< ver.get_major() << '.'
			<< ver.get_minor() << '.'
			<< ver.get_patch();
	}

private:
	std::uint32_t m_major;
	std::uint32_t m_minor;
	std::uint32_t m_patch;
};

} // namespace vitrio

#include "version.inl"
