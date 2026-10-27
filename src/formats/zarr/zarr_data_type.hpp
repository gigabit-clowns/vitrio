// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>

#include <memory/byte_order.hpp>

#include <nlohmann/json.hpp>

#include <string>

namespace vitrio
{
namespace zarr
{

/**
 * @brief The type of the values of a ZARR array, and the order of their
 * bytes in a decoded chunk.
 */
struct zarr_data_type
{
	numerical_type type;
	byte_order order;
};

/**
 * @brief Read the data type of an array of version 2, a typestring of
 * NumPy such as "<f4".
 *
 * The first character is the order of the bytes, little endian for "<", big
 * endian for ">" and of no matter for "|", which only a type of one byte
 * may state.
 *
 * @param dtype The value of "dtype".
 * @param where What a message starts with.
 * @return zarr_data_type The type and its order. One of a single byte is
 * given the order of the system.
 * @throws image_file_format_error If @p dtype is no typestring.
 * @throws unsupported_operation_error If it is one of what no image holds:
 * a structured type, a string, a date or a time, or an object.
 */
zarr_data_type parse_v2_data_type(
	const nlohmann::json &dtype,
	const std::string &where
);

/**
 * @brief Read the data type of an array of version 3, such as "float32".
 *
 * Version 3 states the order of the bytes in the codec that turns an array
 * into bytes, rather than in the type.
 *
 * @param data_type The value of "data_type".
 * @param order The order the bytes codec states.
 * @param where What a message starts with.
 * @return zarr_data_type The type and @p order.
 * @throws image_file_format_error If @p data_type is neither a name nor an
 * object that names a type.
 * @throws unsupported_operation_error If it names another type than a
 * boolean, an integer, a floating point or a complex number.
 */
zarr_data_type parse_v3_data_type(
	const nlohmann::json &data_type,
	byte_order order,
	const std::string &where
);

} // namespace zarr
} // namespace vitrio
