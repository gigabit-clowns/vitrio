// SPDX-License-Identifier: LGPL-2.1-or-later

#include "page_size.hpp"

#include <windows.h>

namespace vitrio
{

std::size_t get_page_size() noexcept
{
	SYSTEM_INFO information;
	::GetSystemInfo(&information);

	return information.dwPageSize;
}

} // namespace vitrio
