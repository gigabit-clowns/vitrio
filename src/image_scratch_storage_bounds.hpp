// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>

namespace vitrio
{

class array_descriptor;

/**
 * @brief Check that bytes of the block of a storage lie within it.
 *
 * @param offset Where the bytes start, from the first of the block.
 * @param count How many bytes there are.
 * @param size How many bytes the block has.
 * @param name What the message names as refusing them.
 * @throws std::out_of_range If the bytes reach past the block.
 */
void check_bytes_within(
	std::size_t offset,
	std::size_t count,
	std::size_t size,
	const char *name
);

/**
 * @brief Check that values a block holds lie within it.
 *
 * @param values Where the values lie, their offset counted in elements from
 * the first byte of the block.
 * @param size How many bytes the block has.
 * @param name What the message names as refusing them.
 * @throws std::invalid_argument If @p values is not initialized.
 * @throws std::out_of_range If a value lies outside the block.
 */
void check_values_within(
	const array_descriptor &values,
	std::size_t size,
	const char *name
);

} // namespace vitrio
