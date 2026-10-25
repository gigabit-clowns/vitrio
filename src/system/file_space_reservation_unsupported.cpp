// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_space_reservation.hpp"

namespace vitrio
{

// The room is taken as the file is written.
void reserve_file_space(
	file_handle_t,
	std::uint64_t,
	std::error_code &error
) noexcept
{
	error.clear();
}

} // namespace vitrio
