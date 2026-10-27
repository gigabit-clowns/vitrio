// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_handle.hpp"

#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>
#include <system_error>

namespace vitrio
{

/**
 * @brief Read a stretch of a file into buffers, in order, from the first.
 *
 * The read fills the buffers, and stops short of filling them only where the
 * file ends.
 *
 * @param file The file to read.
 * @param offset Where in the file the stretch starts, in bytes.
 * @param buffers Where its bytes go: as many as the stretch covers.
 * @param error Set to what the system failed the read with, and cleared
 * where it succeeded.
 * @return std::size_t How many bytes were read. Zero where the read failed.
 */
std::size_t read_scattered(
	file_handle_t file,
	std::uint64_t offset,
	span<const memory_range> buffers,
	std::error_code &error
) noexcept;

} // namespace vitrio
