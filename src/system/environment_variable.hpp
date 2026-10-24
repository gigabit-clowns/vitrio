// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <boost/optional.hpp>

#include <string>

namespace vitrio
{

/**
 * @brief Get the value of a variable of the environment of the process.
 *
 * @param name The name of the variable.
 * @return boost::optional<std::string> Its value, or none if it is not set.
 */
boost::optional<std::string> get_environment_variable(const char *name);

} // namespace vitrio
