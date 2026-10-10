// SPDX-License-Identifier: LGPL-2.1-or-later

#include "eer_extensions.hpp"

namespace vitrio
{
namespace eer
{

bool is_extension(const std::string &extension) noexcept
{
	return extension == ".eer";
}

} // namespace eer
} // namespace vitrio
