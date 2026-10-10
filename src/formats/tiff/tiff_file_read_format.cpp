// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tiff_file_read_format.hpp"

#include "tiff_reader.hpp"
#include "tiff_signature.hpp"

#include <vitrio/image_file_probe.hpp>

#include <formats/eer/eer_extensions.hpp>
#include <formats/image_file_format_registration_macros.hpp>

namespace vitrio
{
namespace tiff
{

std::string tiff_file_read_format::get_name() const
{
	return "TIFF";
}

image_file_format_suitability
tiff_file_read_format::get_suitability(const image_file_probe &probe) const
{
	// An EER file is a TIFF file whose pages hold events, which are read
	// as such rather than as samples this format can not decode.
	return has_signature(probe.get_leading_bytes()) &&
		!eer::is_extension(probe.get_extension())
		? image_file_format_suitability::normal
		: image_file_format_suitability::unsupported;
}

std::shared_ptr<image_reader>
tiff_file_read_format::open(const image_file_probe &probe) const
{
	return std::make_shared<tiff_reader>(probe.get_path());
}

VITRIO_REGISTER_IMAGE_FILE_READ_FORMAT(
	tiff,
	vitrio::tiff::tiff_file_read_format
);

} // namespace tiff
} // namespace vitrio
