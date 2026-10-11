// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_extensions.hpp"

namespace vitrio
{
namespace dm
{

bool is_readable_extension(const std::string &extension) noexcept
{
	return extension == ".dm3" || extension == ".dm4";
}

} // namespace dm
} // namespace vitrio
