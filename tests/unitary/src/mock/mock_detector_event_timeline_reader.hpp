// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_timeline_reader.hpp>

#include <vitrio/detector_event_timeline.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>

#include <cstdint>
#include <trompeloeil.hpp>

namespace vitrio
{

class mock_detector_event_timeline_reader final
	: public detector_event_timeline_reader
{
public:
	mock_detector_event_timeline_reader() = default;

	MAKE_CONST_MOCK0(
		get_descriptor,
		const detector_event_timeline_descriptor&(),
		noexcept override
	);

	MAKE_CONST_MOCK3(
		read,
		void(
			std::uint64_t time_begin,
			std::uint64_t time_end,
			detector_event_timeline &destination
		),
		override
	);
};

} // namespace vitrio
