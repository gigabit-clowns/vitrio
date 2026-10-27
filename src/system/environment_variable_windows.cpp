// SPDX-License-Identifier: LGPL-2.1-or-later

#include "environment_variable.hpp"

#include <windows.h>

namespace vitrio
{

boost::optional<std::string> get_environment_variable(const char *name)
{
	// Asked for with no room, the system answers with the room the value
	// takes, its terminator included. The value may change in between, so
	// it is asked for again until it fits.
	std::string value;
	DWORD size = ::GetEnvironmentVariableA(name, nullptr, 0);
	while (size != 0)
	{
		// An empty value is answered with nothing too, so only the error
		// tells it from a variable that went meanwhile.
		value.resize(size);
		::SetLastError(ERROR_SUCCESS);
		const auto length = ::GetEnvironmentVariableA(name, &value[0], size);
		if (length == 0 && ::GetLastError() == ERROR_ENVVAR_NOT_FOUND)
		{
			break;
		}

		if (length < size)
		{
			value.resize(length);
			return value;
		}
		size = length;
	}

	return boost::none;
}

} // namespace vitrio
