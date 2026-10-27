// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_handle.hpp"

#include <cstdint>
#include <system_error>

namespace vitrio
{

/**
 * @brief Reserve the room a file takes on its disk, where its file system
 * can, so that writing it later does not find the disk full.
 *
 * What the file holds is left as it is. Where the file system or the system
 * can not reserve room, nothing is reserved, and that is no failure: the
 * room is then taken as the file is written.
 *
 * @param file The file, open for writing.
 * @param size How many bytes, from the first of the file, to reserve.
 * @param error Set to what the system failed the reservation with, such as
 * finding the disk full, and cleared otherwise.
 */
void reserve_file_space(
	file_handle_t file,
	std::uint64_t size,
	std::error_code &error
) noexcept;

} // namespace vitrio
