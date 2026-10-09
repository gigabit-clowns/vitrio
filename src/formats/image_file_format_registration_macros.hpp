// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_file_format_registration.hpp>

#include <builtin_image_file_format_registry.hpp>

/**
 * @brief Instantiate and auto-register an image read format.
 *
 * Expanded at namespace scope, it adds the format to the registry of the
 * formats bundled with the library during static initialization.
 *
 * @param name Identifier of the registration object.
 * @param ... The format type. It comes last so that the commas in its
 * template arguments do not split the macro arguments.
 */
#define VITRIO_REGISTER_IMAGE_FILE_READ_FORMAT(name, ...) \
	static const ::vitrio::image_file_format_registration< \
		__VA_ARGS__, \
		::vitrio::image_file_read_format_registry \
	> name##_image_file_read_format_registration( \
		::vitrio::get_builtin_image_file_read_format_registry() \
	)

/**
 * @brief Instantiate and auto-register an image write format.
 *
 * @param name Identifier of the registration object.
 * @param ... The format type.
 *
 * @see VITRIO_REGISTER_IMAGE_FILE_READ_FORMAT
 */
#define VITRIO_REGISTER_IMAGE_FILE_WRITE_FORMAT(name, ...) \
	static const ::vitrio::image_file_format_registration< \
		__VA_ARGS__, \
		::vitrio::image_file_write_format_registry \
	> name##_image_file_write_format_registration( \
		::vitrio::get_builtin_image_file_write_format_registry() \
	)
