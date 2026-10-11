// SPDX-License-Identifier: LGPL-2.1-or-later

#include "detector_event_binning.hpp"

#include "index_tuple.hpp"

#include <vitrio/detector_event_binning.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/span.hpp>

#include <nanobind/operators.h>
#include <nanobind/stl/string.h>

#include <cstddef>
#include <functional>
#include <string>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

index_tuple resolve(
	const detector_event_binning &self,
	const detector_event_timeline_descriptor &events
)
{
	const auto bin = self.resolve(events);
	return to_tuple(make_span(bin));
}

std::size_t hash(const detector_event_binning &self)
{
	return std::hash<std::size_t>()(self.get_factor());
}

std::string to_repr(const detector_event_binning &self)
{
	return "DetectorEventBinning(factor=" +
		std::to_string(self.get_factor()) + ")";
}

} // anonymous namespace

void bind_detector_event_binning(nb::module_ &m)
{
	nb::class_<detector_event_binning>(m, "DetectorEventBinning")
		.def(nb::init<std::size_t>(), nb::arg("factor"))
		.def(nb::self == nb::self)
		.def(nb::self != nb::self)
		.def("__hash__", &hash)
		.def("__repr__", &to_repr)
		.def_prop_ro("factor", &detector_event_binning::get_factor)
		.def("resolve", &resolve, nb::arg("events"));
}

} // namespace vitrio
