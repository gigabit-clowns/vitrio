// SPDX-License-Identifier: LGPL-2.1-or-later

#include "builtin_image_file_format_registry.hpp"

namespace vitrio
{

image_file_read_format_registry&
get_builtin_image_file_read_format_registry() noexcept
{
	static image_file_read_format_registry registry;
	return registry;
}

image_file_write_format_registry&
get_builtin_image_file_write_format_registry() noexcept
{
	static image_file_write_format_registry registry;
	return registry;
}

} // namespace vitrio
