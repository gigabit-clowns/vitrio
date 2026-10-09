// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tiff_extensions.hpp"

namespace vitrio
{
namespace tiff
{

bool is_writable_extension(const std::string &extension) noexcept
{
	return extension == ".tif" || extension == ".tiff";
}

} // namespace tiff
} // namespace vitrio
