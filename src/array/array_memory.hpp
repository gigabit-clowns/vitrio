// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstddef>

namespace vitrio
{

class array_descriptor;

/**
 * @brief Get how many bytes of memory the elements of a descriptor take.
 *
 * The bytes are counted from the first byte of the memory through the last
 * element the descriptor addresses, so what lies before the element at the
 * origin is counted too.
 *
 * @param descriptor The descriptor. Must be initialized.
 * @return std::size_t The size in bytes. Zero when the descriptor addresses
 * no element.
 * @throws std::out_of_range If an element lies before the first byte of the
 * memory, or if the elements span more than can be addressed.
 */
std::size_t compute_storage_requirement(const array_descriptor &descriptor);

/**
 * @brief Check that a block of memory can be read as a descriptor states.
 *
 * @param memory The memory.
 * @param descriptor The descriptor.
 * @throws std::invalid_argument If @p descriptor is not initialized, or if
 * @p memory is not aligned to the elements.
 * @throws std::out_of_range If an element @p descriptor addresses lies
 * outside @p memory.
 */
void check_array_memory(
	span<const byte> memory,
	const array_descriptor &descriptor
);

} // namespace vitrio
