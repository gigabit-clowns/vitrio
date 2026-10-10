// SPDX-License-Identifier: LGPL-2.1-or-later

#include "detector_event_timeline_reader.hpp"

#include "guarded_detector_event_timeline.hpp"

#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/detector_event_timeline_reader.hpp>

#include <cstdint>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

void read(
	const detector_event_timeline_reader &self,
	std::uint64_t time_begin,
	std::uint64_t time_end,
	guarded_detector_event_timeline &destination
)
{
	const guarded_detector_event_timeline::write_guard write(destination);
	const nb::gil_scoped_release release;
	self.read(time_begin, time_end, write.get());
}

} // anonymous namespace

void bind_detector_event_timeline_reader(nb::module_ &m)
{
	nb::class_<detector_event_timeline_reader>(
		m,
		"DetectorEventTimelineReader"
	)
		.def_prop_ro(
			"descriptor",
			&detector_event_timeline_reader::get_descriptor
		)
		.def(
			"read", &read,
			nb::arg("time_begin"), nb::arg("time_end"),
			nb::arg("destination")
		);
}

} // namespace vitrio
