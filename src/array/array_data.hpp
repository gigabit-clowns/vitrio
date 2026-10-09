// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>

namespace vitrio
{

class array_ref;
class const_array_ref;

/**
 * @brief Get where an array holds its elements, to write them.
 *
 * @param array The array to reach.
 * @return byte* The first byte of its memory, never null.
 * @throws std::invalid_argument If @p array is not initialized.
 */
byte* get_array_data(array_ref array);

/**
 * @brief Get where an array holds its elements, to read them.
 *
 * @param array The array to reach.
 * @return const byte* The first byte of its memory, never null.
 * @throws std::invalid_argument If @p array is not initialized.
 */
const byte* get_array_data(const_array_ref array);

} // namespace vitrio
