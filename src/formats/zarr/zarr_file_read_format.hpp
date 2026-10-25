// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_file_read_format.hpp>

namespace vitrio
{
namespace zarr
{

/**
 * @brief The ability of the ZARR format, versions 2 and 3, to be read.
 *
 * A ZARR array is a directory, claimed when it holds the metadata of ZARR,
 * whatever its extension: zarr.json, .zarray or .zgroup, which is looked for
 * by name and not read.
 *
 * A key may name an array, or a group that is an image of OME-Zarr, 0.4 or
 * 0.5, which opens the finest of its levels. An array that is a level of
 * such an image takes its axes from the group above it. How many trailing
 * axes are one image or volume is decided as decide_core_rank says.
 *
 * Arrays are read in a regular grid of chunks, unsharded, with the codecs of
 * zarr_codec_pipeline. Reading one that holds anything else is reported as
 * an unsupported_operation_error.
 */
class zarr_file_read_format final
	: public image_file_read_format
{
public:
	zarr_file_read_format() noexcept = default;

	~zarr_file_read_format() override = default;

	std::string get_name() const override;

	image_file_format_suitability
	get_suitability(const image_file_probe &probe) const override;

	std::shared_ptr<image_reader> open(
		const image_file_probe &probe,
		const file_transfer_pool &transfers
	) const override;
};

} // namespace zarr
} // namespace vitrio
