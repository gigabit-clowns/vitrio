// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_read_format_manager.hpp>
#include <vitrio/image_write_format_manager.hpp>

#include <memory>

namespace vitrio
{
namespace test
{

/**
 * @brief Make a manager of the formats files are read with, holding those
 * bundled with the library.
 */
inline std::shared_ptr<image_read_format_manager> make_builtin_read_formats()
{
	auto formats = std::make_shared<image_read_format_manager>();
	formats->register_builtin_formats();

	return formats;
}

/**
 * @brief Make a manager of the formats files are written with, holding those
 * bundled with the library.
 */
inline std::shared_ptr<image_write_format_manager>
make_builtin_write_formats()
{
	auto formats = std::make_shared<image_write_format_manager>();
	formats->register_builtin_formats();

	return formats;
}

} // namespace test
} // namespace vitrio
