// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_renderer.hpp>

#include <vitrio/array/array_ref.hpp>
#include <vitrio/detector_event_timeline.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>

#include <trompeloeil.hpp>

namespace vitrio
{

class mock_detector_event_renderer final
	: public detector_event_renderer
{
public:
	mock_detector_event_renderer() = default;

	MAKE_CONST_MOCK1(
		get_image_extents,
		std::vector<std::size_t>(
			const detector_event_timeline_descriptor &events
		),
		override
	);

	MAKE_CONST_MOCK0(get_data_type, numerical_type(), noexcept override);

	MAKE_CONST_MOCK3(
		render,
		void(
			const detector_event_timeline_descriptor &descriptor,
			const detector_event_timeline &events,
			array_ref destination
		),
		override
	);
};

} // namespace vitrio
