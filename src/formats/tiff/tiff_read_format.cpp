// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tiff_read_format.hpp"

#include "tiff_reader.hpp"
#include "tiff_signature.hpp"

#include <vitrio/image_probe.hpp>

#include <formats/image_format_registration_macros.hpp>

namespace vitrio
{
namespace tiff
{

std::string tiff_read_format::get_name() const
{
	return "TIFF";
}

image_format_suitability
tiff_read_format::get_suitability(const image_probe &probe) const
{
	return has_signature(probe.get_leading_bytes())
		? image_format_suitability::normal
		: image_format_suitability::unsupported;
}

std::shared_ptr<image_reader>
tiff_read_format::open(const image_probe &probe) const
{
	return std::make_shared<tiff_reader>(probe.get_path());
}

VITRIO_REGISTER_IMAGE_READ_FORMAT(tiff, vitrio::tiff::tiff_read_format);

} // namespace tiff
} // namespace vitrio
