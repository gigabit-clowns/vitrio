// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/library_version.hpp>

namespace vitrio
{

version get_library_version() noexcept
{
	return version(
		VITRIO_VERSION_MAJOR,
		VITRIO_VERSION_MINOR,
		VITRIO_VERSION_PATCH
	);
}

} // namespace vitrio
