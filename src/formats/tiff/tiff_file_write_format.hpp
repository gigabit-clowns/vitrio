// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_file_write_format.hpp>

namespace vitrio
{
namespace tiff
{

/**
 * @brief The ability of the TIFF format to be written.
 *
 * A file is claimed on its extension alone, since it need not exist yet and
 * so has no signature to recognize.
 *
 * A file is created for one image or for a stack of several, none of whose
 * extents is zero. A stack of one image is not created, since a file of one
 * page reads back as an image, and neither is anything whose core rank is
 * not two, since the pages of a file are images.
 */
class tiff_file_write_format final
	: public image_file_write_format
{
public:
	tiff_file_write_format() noexcept = default;

	~tiff_file_write_format() override = default;

	std::string get_name() const override;

	image_file_format_suitability
	get_suitability(const image_file_probe &probe) const override;

	std::shared_ptr<image_writer> open(
		const image_file_probe &probe,
		const image_descriptor &descriptor,
		const image_metadata &metadata
	) const override;
};

} // namespace tiff
} // namespace vitrio
