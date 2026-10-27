// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief Read the value a ZARR array holds where it stores no chunk, as one
 * element of its type in the byte order of the system.
 *
 * A number is taken as JSON has it, and must fit the type. A floating point
 * type also takes "NaN", "Infinity" and "-Infinity", and the bits of a value
 * written in hexadecimal, "0x" and two digits a byte. A complex type takes a
 * pair, its real part first, each as a floating point type does. A boolean
 * takes true and false, and version 2 writes those as 1 and 0 too. Version 2
 * may state no value, null, which leaves what such chunks hold undefined:
 * zero is what they are read as.
 *
 * @param value The value of "fill_value".
 * @param type The data type of the array.
 * @param where What a message starts with.
 * @return std::vector<byte> The bytes of one element, a stored_boolean for a
 * boolean.
 * @throws image_file_format_error If @p value is none of the above for
 * @p type, or a number that @p type can not hold.
 */
std::vector<byte> parse_fill_value(
	const nlohmann::json &value,
	numerical_type type,
	const std::string &where
);

} // namespace zarr
} // namespace vitrio
