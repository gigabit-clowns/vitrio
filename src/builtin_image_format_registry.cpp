// SPDX-License-Identifier: LGPL-2.1-or-later

#include "builtin_image_format_registry.hpp"

namespace vitrio
{

image_read_format_registry& get_builtin_image_read_format_registry() noexcept
{
	static image_read_format_registry registry;
	return registry;
}

image_write_format_registry& get_builtin_image_write_format_registry() noexcept
{
	static image_write_format_registry registry;
	return registry;
}

} // namespace vitrio
