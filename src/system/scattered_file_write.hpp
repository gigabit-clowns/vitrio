// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_handle.hpp"

#include <vitrio/const_memory_range.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>
#include <system_error>

namespace vitrio
{

/**
 * @brief Write a stretch of a file from buffers, in order, from the first.
 *
 * The write takes every byte of the buffers unless it fails.
 *
 * @param file The file to write, open for writing.
 * @param offset Where in the file the stretch starts, in bytes.
 * @param buffers Where its bytes come from: as many as the stretch covers.
 * @param error Set to what the system failed the write with, and cleared
 * where it succeeded.
 * @return std::size_t How many bytes were written. Zero where the write
 * failed.
 */
std::size_t write_scattered(
	file_handle_t file,
	std::uint64_t offset,
	span<const const_memory_range> buffers,
	std::error_code &error
) noexcept;

} // namespace vitrio
