// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_timeline_reader_provider.hpp>

#include <vitrio/detector_event_timeline_reader.hpp>

#include <trompeloeil.hpp>

namespace vitrio
{

class mock_detector_event_timeline_reader_provider final
	: public detector_event_timeline_reader_provider
{
public:
	mock_detector_event_timeline_reader_provider() = default;

	MAKE_MOCK1(
		acquire,
		std::shared_ptr<const detector_event_timeline_reader>(
			const std::string &key
		),
		override
	);
};

} // namespace vitrio
