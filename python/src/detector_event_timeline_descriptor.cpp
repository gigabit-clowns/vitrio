// SPDX-License-Identifier: LGPL-2.1-or-later

#include "detector_event_timeline_descriptor.hpp"

#include "index_tuple.hpp"

#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/span.hpp>

#include <nanobind/operators.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <cstddef>
#include <cstdint>
#include <new>
#include <sstream>
#include <string>
#include <vector>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

void construct(
	detector_event_timeline_descriptor *self,
	const std::vector<std::size_t> &extents,
	const std::vector<std::size_t> &subdivisions,
	std::uint64_t time_extent,
	double time_quantum
)
{
	new (self) detector_event_timeline_descriptor(
		make_span(extents),
		make_span(subdivisions),
		time_extent,
		time_quantum
	);
}

index_tuple get_extents(const detector_event_timeline_descriptor &self)
{
	return to_tuple(self.get_extents());
}

index_tuple get_subdivisions(const detector_event_timeline_descriptor &self)
{
	return to_tuple(self.get_subdivisions());
}

std::string to_repr(nb::handle self)
{
	std::ostringstream oss;
	oss << "DetectorEventTimelineDescriptor(extents="
		<< nb::repr(self.attr("extents")).c_str()
		<< ", subdivisions=" << nb::repr(self.attr("subdivisions")).c_str()
		<< ", time_extent=" << nb::repr(self.attr("time_extent")).c_str()
		<< ", time_quantum=" << nb::repr(self.attr("time_quantum")).c_str()
		<< ")";
	return oss.str();
}

} // anonymous namespace

void bind_detector_event_timeline_descriptor(nb::module_ &m)
{
	nb::class_<detector_event_timeline_descriptor>(
		m,
		"DetectorEventTimelineDescriptor",
		"What is fixed for one acquisition of a detector that counts events.\n"
		"\n"
		"The positions of its events lie on a grid of position quanta: a "
		"pixel of the detector may be divided into several of them along each "
		"axis. The time of the acquisition is counted in time quanta, which "
		"are the frames of an EER file."
	)
		.def(
			"__init__", &construct,
			nb::arg("extents"), nb::arg("subdivisions"),
			nb::arg("time_extent"), nb::arg("time_quantum"),
			"Describe an acquisition.\n"
			"\n"
			"Args:\n"
			"    extents: The extents of the grid of positions, slowest axis "
			"first.\n"
			"    subdivisions: How many position quanta a pixel of the "
			"detector spans along each axis.\n"
			"    time_extent: How many time quanta the acquisition spans.\n"
			"    time_quantum: The length of a time quantum in seconds, or "
			"zero when it is not known."
		)
		.def(nb::self == nb::self)
		.def(nb::self != nb::self)
		.def("__repr__", &to_repr)
		.def_prop_ro(
			"rank", &detector_event_timeline_descriptor::get_rank,
			"How many axes events are placed along."
		)
		.def_prop_ro(
			"extents", &get_extents,
			"The extents of the grid of positions, as a tuple."
		)
		.def_prop_ro(
			"subdivisions", &get_subdivisions,
			"How many position quanta a pixel spans along each axis."
		)
		.def_prop_ro(
			"time_extent",
			&detector_event_timeline_descriptor::get_time_extent,
			"How many time quanta the acquisition spans."
		)
		.def_prop_ro(
			"time_quantum",
			&detector_event_timeline_descriptor::get_time_quantum,
			"The length of a time quantum in seconds, or zero if unknown."
		);
}

} // namespace vitrio
