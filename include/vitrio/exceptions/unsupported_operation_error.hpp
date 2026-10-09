// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <stdexcept>

#include <vitrio/export.hpp>
#include <vitrio/platform.hpp>

namespace vitrio
{

/**
 * @brief Exception indicating that nothing available can carry out a
 * requested operation.
 *
 * Thrown when the implementations at hand, such as the registered formats,
 * support none of what was asked for: a file of that kind, a conversion
 * between those data types.
 */
VITRIO_STD_BASE_INTERFACE
class VITRIO_API unsupported_operation_error : public std::runtime_error
{
	using runtime_error::runtime_error;
};

} // namespace vitrio
