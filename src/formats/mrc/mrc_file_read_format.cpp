// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mrc_file_read_format.hpp"

#include "mrc_constants.hpp"
#include "mrc_extensions.hpp"
#include "mrc_geometry.hpp"
#include "mrc_header.hpp"

#include <vitrio/image_file_probe.hpp>
#include <vitrio/image_metadata.hpp>

#include <formats/image_file_format_registration_macros.hpp>
#include <formats/memory_mapping/mapped_image_reader.hpp>

namespace vitrio
{
namespace mrc
{

std::string mrc_file_read_format::get_name() const
{
	return "MRC";
}

image_file_format_suitability
mrc_file_read_format::get_suitability(const image_file_probe &probe) const
{
	if (has_map_identifier(probe.get_leading_bytes()))
	{
		return image_file_format_suitability::normal;
	}

	if (is_readable_extension(probe.get_extension()) &&
		probe.get_leading_bytes().size() >= header_size)
	{
		return image_file_format_suitability::fallback;
	}

	return image_file_format_suitability::unsupported;
}

std::shared_ptr<image_reader>
mrc_file_read_format::open(const image_file_probe &probe) const
{
	const auto header = parse_header(probe.get_leading_bytes());

	return std::make_shared<mapped_image_reader>(
		probe.get_path(),
		derive_file_layout(
			header,
			get_single_section(probe.get_extension())
		),
		image_metadata()
	);
}

VITRIO_REGISTER_IMAGE_FILE_READ_FORMAT(mrc, vitrio::mrc::mrc_file_read_format);

} // namespace mrc
} // namespace vitrio
