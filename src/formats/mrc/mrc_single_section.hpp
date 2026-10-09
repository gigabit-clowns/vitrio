// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace vitrio
{
namespace mrc
{

/**
 * @brief What a file of one section in the image space group holds.
 *
 * MRC2014 states a single image and a stack of one image with the same
 * header, space group 0 and one section, so which one such a file holds is
 * decided outside the header.
 */
enum class mrc_single_section
{
	/// A single image, which is how the standard reads such a header alone.
	image,

	/// A stack of one image.
	image_stack
};

} // namespace mrc
} // namespace vitrio
