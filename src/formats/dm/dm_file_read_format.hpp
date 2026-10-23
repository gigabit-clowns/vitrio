// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_file_read_format.hpp>

namespace vitrio
{
namespace dm
{

/**
 * @brief The ability of the DigitalMicrograph format, of versions 3 and 4,
 * to be read.
 *
 * Neither version carries an identifier: a file begins with its version, a
 * 32-bit field of three zeros and a 3 or 4. A file that begins as one of
 * them does (@ref has_dm_signature) is claimed when its extension is `.dm3`
 * or `.dm4`, and only at @ref image_file_format_suitability::fallback
 * otherwise, so that a format that recognizes a file by an identifier of
 * its own takes it first.
 *
 * Opening a file walks the whole of its tags, which run well past the
 * leading bytes of a probe, so the file is mapped to be parsed. Its image is
 * the one @ref locate_image finds, and is read through the reader
 * @ref open_raw_image_reader opens for it, aligned or not.
 *
 * Versions 2, an older layout, and 5, an HDF5 file, are not read.
 */
class dm_file_read_format final
	: public image_file_read_format
{
public:
	dm_file_read_format() noexcept = default;

	~dm_file_read_format() override = default;

	std::string get_name() const override;

	image_file_format_suitability
	get_suitability(const image_file_probe &probe) const override;

	std::shared_ptr<image_reader> open(
		const image_file_probe &probe,
		const file_transfer_pool &transfers
	) const override;
};

} // namespace dm
} // namespace vitrio
