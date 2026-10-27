// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_file_write_format.hpp>

namespace vitrio
{
namespace zarr
{

/**
 * @brief The ability of the ZARR format, version 3, to be written.
 *
 * A path is claimed by its extension, .zarr, or when it is a directory that
 * holds the metadata of ZARR already. What is written is an image of
 * OME-Zarr 0.5 where OME-Zarr can state it, a group of one level, the array
 * "0", at a scale of one: images and volumes, and stacks of them, of five
 * axes at most. Anything else is a plain array. Either way the array says
 * the core rank of the image, so that it reads back as it was written.
 *
 * The chunks of an image or volume are as make_array_metadata says, as wide
 * as VITRIO_DEFAULT_ZARR_IMAGE_CHUNK_EDGE or
 * VITRIO_DEFAULT_ZARR_VOLUME_CHUNK_EDGE, each image of a stack in chunks of
 * its own.
 *
 * Opening replaces what is at the path: a file, or a directory that is a
 * node of ZARR, with all it holds. Any other directory that holds anything
 * is refused rather than removed.
 */
class zarr_file_write_format final
	: public image_file_write_format
{
public:
	zarr_file_write_format() noexcept = default;

	~zarr_file_write_format() override = default;

	std::string get_name() const override;

	image_file_format_suitability
	get_suitability(const image_file_probe &probe) const override;

	std::shared_ptr<image_writer> open(
		const image_file_probe &probe,
		const image_descriptor &descriptor,
		const image_metadata &metadata,
		const file_transfer_pool &transfers
	) const override;
};

} // namespace zarr
} // namespace vitrio
