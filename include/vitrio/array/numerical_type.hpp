// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <cstddef>
#include <ostream>

namespace vitrio
{

/**
 * @brief The type of the elements an array or an image file holds.
 *
 * A complex type is named after the floating point type of each of its two
 * components, so @c complex_float32 is a pair of @c float32.
 */
enum class numerical_type
{
	unknown = -1,

	boolean,

	int8,
	uint8,
	int16,
	uint16,
	int32,
	uint32,
	int64,
	uint64,

	float16,
	float32,
	float64,

	complex_float16,
	complex_float32,
	complex_float64
};

/**
 * @brief Get the size of one element of a numerical type.
 *
 * @param type The type.
 * @return std::size_t The size in bytes. Zero when @p type is unknown.
 */
VITRIO_API
std::size_t get_size(numerical_type type) noexcept;

/**
 * @brief Get the name of a numerical type.
 *
 * @param type The type.
 * @return const char* The name, as its enumerator is spelled. An empty
 * string when @p type is unknown.
 */
VITRIO_API
const char* to_string(numerical_type type) noexcept;

/**
 * @brief Write the name of a numerical type to a stream.
 *
 * @param os The stream.
 * @param type The type.
 * @return std::ostream& The stream.
 *
 * @see to_string
 */
VITRIO_API
std::ostream& operator<<(std::ostream &os, numerical_type type);

} // namespace vitrio
