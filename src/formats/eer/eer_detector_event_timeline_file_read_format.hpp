// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_timeline_file_read_format.hpp>

namespace vitrio
{
namespace eer
{

/**
 * @brief The ability of the EER format to be read.
 *
 * An EER file is a TIFF file, so it begins with the signature of one, and it
 * is told from any other by its extension: the private tags that tell it by
 * its contents may lie anywhere in the file, past the bytes a probe holds.
 */
class eer_detector_event_timeline_file_read_format final
	: public detector_event_timeline_file_read_format
{
public:
	eer_detector_event_timeline_file_read_format() noexcept = default;

	~eer_detector_event_timeline_file_read_format() override = default;

	std::string get_name() const override;

	image_file_format_suitability
	get_suitability(const image_file_probe &probe) const override;

	std::shared_ptr<detector_event_timeline_reader> open(
		const image_file_probe &probe
	) const override;
};

} // namespace eer
} // namespace vitrio
