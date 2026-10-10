// SPDX-License-Identifier: LGPL-2.1-or-later

#include "eer_detector_event_timeline_file_read_format.hpp"

#include "eer_detector_event_timeline_reader.hpp"
#include "eer_extensions.hpp"

#include <formats/image_file_format_registration_macros.hpp>
#include <formats/tiff/tiff_signature.hpp>

#include <vitrio/image_file_probe.hpp>

namespace vitrio
{
namespace eer
{

std::string eer_detector_event_timeline_file_read_format::get_name() const
{
	return "EER";
}

image_file_format_suitability
eer_detector_event_timeline_file_read_format::get_suitability(
	const image_file_probe &probe
) const
{
	return is_extension(probe.get_extension()) &&
		tiff::has_signature(probe.get_leading_bytes())
		? image_file_format_suitability::normal
		: image_file_format_suitability::unsupported;
}

std::shared_ptr<detector_event_timeline_reader>
eer_detector_event_timeline_file_read_format::open(
	const image_file_probe &probe
) const
{
	return std::make_shared<eer_detector_event_timeline_reader>(
		probe.get_path());
}

VITRIO_REGISTER_DETECTOR_EVENT_TIMELINE_FILE_READ_FORMAT(
	eer,
	vitrio::eer::eer_detector_event_timeline_file_read_format
);

} // namespace eer
} // namespace vitrio
