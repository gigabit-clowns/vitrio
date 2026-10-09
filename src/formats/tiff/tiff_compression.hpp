// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace vitrio
{
namespace tiff
{

/**
 * @brief How the samples of a page are encoded when it is written.
 */
enum class tiff_compression
{
	none,
	lzw,
};

} // namespace tiff
} // namespace vitrio
