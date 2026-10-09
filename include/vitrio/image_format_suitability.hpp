// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <limits>
#include <ostream>

namespace vitrio
{

/**
 * @brief How well an image format fits a file.
 *
 * When several formats recognize one file, the one that reports the highest
 * suitability is used.
 */
enum class image_format_suitability
{
	/// The format does not recognize the file.
	unsupported = std::numeric_limits<int>::min(),
	/// The format handles the file, but any other that does is preferred.
	fallback = -1024,
	/// The format handles the file. Report this by default.
	normal = 0,
	/// The format handles the file better than one reporting normal.
	optimal = 1024,
};

constexpr bool operator<(
	image_format_suitability lhs,
	image_format_suitability rhs
) noexcept;

constexpr bool operator<=(
	image_format_suitability lhs,
	image_format_suitability rhs
) noexcept;

constexpr bool operator>(
	image_format_suitability lhs,
	image_format_suitability rhs
) noexcept;

constexpr bool operator>=(
	image_format_suitability lhs,
	image_format_suitability rhs
) noexcept;

/**
 * @brief Get the name of a suitability.
 *
 * @param suitability The suitability.
 * @return const char* The name, as its enumerator is spelled. An empty
 * string for a value that is none of the enumerators.
 */
constexpr const char* to_string(image_format_suitability suitability) noexcept;

/**
 * @brief Write the name of a suitability to a stream.
 *
 * @param os The stream.
 * @param suitability The suitability.
 * @return std::basic_ostream<T>& The stream.
 *
 * @see to_string
 */
template <typename T>
std::basic_ostream<T>& operator<<(
	std::basic_ostream<T> &os,
	image_format_suitability suitability
);

} // namespace vitrio

#include "image_format_suitability.inl"
