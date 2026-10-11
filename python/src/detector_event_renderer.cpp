// SPDX-License-Identifier: LGPL-2.1-or-later

#include "detector_event_renderer.hpp"

#include "guarded_detector_event_timeline.hpp"
#include "index_tuple.hpp"

#include <array/array_import.hpp>
#include <array/host_ndarray.hpp>
#include <array/memory_loan.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/detector_event_renderer.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/span.hpp>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

index_tuple get_image_extents(
	const detector_event_renderer &self,
	const detector_event_timeline_descriptor &events
)
{
	const auto extents = self.get_image_extents(events);
	return to_tuple(make_span(extents));
}

void render(
	const detector_event_renderer &self,
	const detector_event_timeline_descriptor &descriptor,
	guarded_detector_event_timeline &events,
	host_ndarray destination
)
{
	const memory_loan loan{const_host_ndarray(destination)};
	auto imported = import_array(destination, loan);
	const guarded_detector_event_timeline::read_guard read(events);
	const nb::gil_scoped_release release;
	self.render(descriptor, read.get(), array_ref(imported));
}

} // anonymous namespace

void bind_detector_event_renderer(nb::module_ &m)
{
	nb::class_<detector_event_renderer>(m, "DetectorEventRenderer")
		.def_prop_ro("data_type", &detector_event_renderer::get_data_type)
		.def(
			"get_image_extents", &get_image_extents,
			nb::arg("events")
		)
		.def(
			"render", &render,
			nb::arg("descriptor"), nb::arg("events"),
			nb::arg("destination").noconvert()
		);
}

} // namespace vitrio
