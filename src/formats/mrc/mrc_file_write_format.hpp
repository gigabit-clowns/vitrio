// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_file_write_format.hpp>

namespace vitrio
{
namespace mrc
{

/**
 * @brief The ability of the MRC format to be written.
 *
 * A file is claimed on its extension alone, since it need not exist yet and
 * so has no header to recognize.
 *
 * A stack of one image can only be created with the extension `.mrcs`, and
 * a single image only with another one, so that the file reads back as it
 * was declared.
 */
class mrc_file_write_format final
	: public image_file_write_format
{
public:
	mrc_file_write_format() noexcept = default;

	~mrc_file_write_format() override = default;

	std::string get_name() const override;

	image_file_format_suitability
	get_suitability(const image_file_probe &probe) const override;

	std::shared_ptr<image_writer> open(
		const image_file_probe &probe,
		const image_descriptor &descriptor,
		const image_metadata &metadata
	) const override;
};

} // namespace mrc
} // namespace vitrio
