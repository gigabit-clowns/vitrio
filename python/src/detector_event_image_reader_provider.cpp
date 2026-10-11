// SPDX-License-Identifier: LGPL-2.1-or-later

#include "detector_event_image_reader_provider.hpp"

#include <vitrio/detector_event_fractionation.hpp>
#include <vitrio/detector_event_image_reader_provider.hpp>
#include <vitrio/detector_event_renderer.hpp>
#include <vitrio/detector_event_timeline_reader_provider.hpp>
#include <vitrio/image_reader_provider.hpp>

#include <nanobind/stl/shared_ptr.h>

#include <memory>

namespace vitrio
{

namespace nb = nanobind;

void bind_detector_event_image_reader_provider(nb::module_ &m)
{
	nb::class_<detector_event_image_reader_provider, image_reader_provider>(
		m,
		"DetectorEventImageReaderProvider"
	)
		.def(
			nb::init<
				std::shared_ptr<detector_event_timeline_reader_provider>,
				detector_event_fractionation,
				std::shared_ptr<const detector_event_renderer>
			>(),
			nb::arg("events"), nb::arg("fractionation"),
			nb::arg("renderer")
		);
}

} // namespace vitrio
