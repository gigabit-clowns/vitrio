// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace vitrio
{

/**
 * @brief The types a value of the metadata of an image file may have.
 *
 * @see image_metadata_value
 */
enum class image_metadata_value_type
{
	/// A whole number, held as a @c std::int64_t.
	integer,
	/// A real number, held as a @c double.
	real,
	/// A text, held as a @c std::string.
	text
};

} // namespace vitrio
