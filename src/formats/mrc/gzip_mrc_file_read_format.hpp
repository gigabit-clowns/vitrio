// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_file_read_format.hpp>

namespace vitrio
{
namespace mrc
{

/**
 * @brief The ability of the MRC format to be read from a file compressed
 * with gzip, such as the `.map.gz` files the EMDB holds.
 *
 * A file is claimed when it begins as a gzip stream does and what its
 * leading bytes inflate into carries the identifier of an MRC header. One
 * whose inflated header lacks the identifier is claimed at
 * @ref image_file_format_suitability::fallback when its name holds an
 * extension of MRC under `.gz`, as `.mrc.gz` does, as the MRC format claims
 * such files uncompressed.
 *
 * Opening a file inflates its header alone. Each read inflates the file
 * again, from its start up to the last value it needs, and nothing inflated
 * is kept: such a file is usually read once, whole.
 *
 * A file holding a single section in the image space group opens as a stack
 * of one image when its name ends in `.mrcs.gz`, and as one image otherwise.
 */
class gzip_mrc_file_read_format final
	: public image_file_read_format
{
public:
	gzip_mrc_file_read_format() noexcept = default;

	~gzip_mrc_file_read_format() override = default;

	std::string get_name() const override;

	image_file_format_suitability
	get_suitability(const image_file_probe &probe) const override;

	std::shared_ptr<image_reader> open(
		const image_file_probe &probe,
		const file_transfer_pool &transfers
	) const override;
};

} // namespace mrc
} // namespace vitrio
