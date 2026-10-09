// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace vitrio
{

/**
 * @brief How a scratch treats what its storage holds when it is
 * constructed.
 */
enum class image_scratch_open_mode
{
	/// The scratch starts with nothing loaded. The storage is not read.
	empty,
	/// The scratch keeps what an earlier scratch of the same layout loaded
	/// into the same storage.
	resumed
};

} // namespace vitrio
