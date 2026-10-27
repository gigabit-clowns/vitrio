// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/const_memory_range.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <sys/uio.h>

#include <cstddef>

namespace vitrio
{

/**
 * @brief Describe what is left of buffers past their first bytes, as the
 * vectors of a scattered read.
 *
 * @param buffers The buffers, in order.
 * @param skipped How many bytes of them, from the first, to leave out.
 * @param vectors Where the vectors are written, from the first: one for each
 * buffer that has bytes left, in order, as many as fit.
 * @param bytes The most bytes the vectors may cover together. Not zero.
 * @return std::size_t How many vectors were written. Zero only where no byte
 * is left.
 */
std::size_t gather_iovecs(
	span<const memory_range> buffers,
	std::size_t skipped,
	span<iovec> vectors,
	std::size_t bytes
) noexcept;

/**
 * @brief Describe what is left of buffers past their first bytes, as the
 * vectors of a gathered write.
 *
 * As the overload for the buffers of a read does. The system only reads
 * through the vectors of a write.
 *
 * @param buffers The buffers, in order.
 * @param skipped How many bytes of them, from the first, to leave out.
 * @param vectors Where the vectors are written, from the first: one for each
 * buffer that has bytes left, in order, as many as fit.
 * @param bytes The most bytes the vectors may cover together. Not zero.
 * @return std::size_t How many vectors were written. Zero only where no byte
 * is left.
 */
std::size_t gather_iovecs(
	span<const const_memory_range> buffers,
	std::size_t skipped,
	span<iovec> vectors,
	std::size_t bytes
) noexcept;

} // namespace vitrio
