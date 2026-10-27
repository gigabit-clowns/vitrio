// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>

namespace vitrio
{

/**
 * @brief A boolean as a file stores it: one byte, false when it is zero and
 * true otherwise.
 *
 * C++ leaves a bool whose byte is neither zero nor one undefined, and a file
 * may hold any byte, so the booleans of a file are read and written through
 * this type rather than as bool. Converting it is what @ref cast does.
 */
struct stored_boolean
{
	std::uint8_t value;
};

static_assert(
	sizeof(stored_boolean) == 1,
	"A boolean of a file must take one byte."
);

} // namespace vitrio
