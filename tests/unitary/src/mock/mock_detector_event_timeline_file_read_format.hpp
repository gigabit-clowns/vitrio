// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_timeline_file_read_format.hpp>

#include <vitrio/detector_event_timeline_reader.hpp>
#include <vitrio/image_file_probe.hpp>

#include <trompeloeil.hpp>

namespace vitrio
{

class mock_detector_event_timeline_file_read_format final
	: public detector_event_timeline_file_read_format
{
public:
	mock_detector_event_timeline_file_read_format() = default;

	MAKE_CONST_MOCK0(get_name, std::string(), override);

	MAKE_CONST_MOCK1(
		get_suitability,
		image_file_format_suitability(const image_file_probe &probe),
		override
	);

	MAKE_CONST_MOCK1(
		open,
		std::shared_ptr<detector_event_timeline_reader>(
			const image_file_probe &probe
		),
		override
	);
};

} // namespace vitrio
