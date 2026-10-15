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
		"DetectorEventImageReaderProvider",
		"Reads the files of a detector that counts events as movies.\n"
		"\n"
		"The time of a file is cut into fractions, and the events of each are "
		"rendered into one image. What it opens is shown as a stack with an "
		"image for each fraction, so it reads wherever an image provider "
		"does. `vitrio.detector_event_image_reader_provider` assembles one."
	)
		.def(
			nb::init<
				std::shared_ptr<detector_event_timeline_reader_provider>,
				detector_event_fractionation,
				std::shared_ptr<const detector_event_renderer>
			>(),
			nb::arg("events"), nb::arg("fractionation"),
			nb::arg("renderer"),
			"Make a provider.\n"
			"\n"
			"Args:\n"
			"    events: The `DetectorEventTimelineReaderProvider` that opens "
			"the files.\n"
			"    fractionation: The `DetectorEventFractionation` that cuts "
			"time.\n"
			"    renderer: The `DetectorEventRenderer` that renders a "
			"fraction."
		);
}

} // namespace vitrio
