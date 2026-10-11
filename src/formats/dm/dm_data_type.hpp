// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>

#include <cstdint>

namespace vitrio
{
namespace dm
{

/**
 * @brief Get the data type of the values of an image from the code its
 * `DataType` tag states.
 *
 * Binary images hold a byte per value and are read as unsigned bytes. Packed
 * complex values, which hold half of a Fourier transform, and RGB values are
 * not read, nor is any code the format is not known to write.
 *
 * @param code The code.
 * @return numerical_type The data type of the values.
 * @throws unsupported_operation_error If the values are not read.
 */
numerical_type get_data_type(std::int64_t code);

} // namespace dm
} // namespace vitrio
