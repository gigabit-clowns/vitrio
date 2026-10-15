// SPDX-License-Identifier: LGPL-2.1-or-later

#include "counting_detector_event_renderer.hpp"

#include <vitrio/counting_detector_event_renderer.hpp>
#include <vitrio/detector_event_binning.hpp>
#include <vitrio/detector_event_renderer.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_counting_detector_event_renderer(nb::module_ &m)
{
	nb::class_<counting_detector_event_renderer, detector_event_renderer>(
		m,
		"CountingDetectorEventRenderer",
		"Renders an image whose pixels count the events that fall on them."
	)
		.def(
			nb::init<detector_event_binning>(), nb::arg("binning"),
			"Make a renderer.\n"
			"\n"
			"Args:\n"
			"    binning: The `DetectorEventBinning` that says how many "
			"pixels of the detector a pixel of the image gathers."
		)
		.def_prop_ro(
			"binning",
			&counting_detector_event_renderer::get_binning,
			"The `DetectorEventBinning` it renders with."
		);
}

} // namespace vitrio
