// SPDX-License-Identifier: LGPL-2.1-or-later

#include "page_size.hpp"

#include <unistd.h>

namespace vitrio
{

std::size_t get_page_size() noexcept
{
	return static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
}

} // namespace vitrio
