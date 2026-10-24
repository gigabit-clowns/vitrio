// SPDX-License-Identifier: LGPL-2.1-or-later

#include "environment_variable.hpp"

#include <cstdlib>

namespace vitrio
{

boost::optional<std::string> get_environment_variable(const char *name)
{
	const char *value = std::getenv(name);
	if (value == nullptr)
	{
		return boost::none;
	}

	return std::string(value);
}

} // namespace vitrio
