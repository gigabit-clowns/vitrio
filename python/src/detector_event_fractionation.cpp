// SPDX-License-Identifier: LGPL-2.1-or-later

#include "detector_event_fractionation.hpp"

#include <vitrio/detector_event_fractionation.hpp>

#include <nanobind/operators.h>
#include <nanobind/stl/string.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

// The library asserts the fraction is in range, which Python can not be
// trusted to keep to.
std::uint64_t get_fraction_begin(
	const detector_event_fractionation &self,
	std::size_t fraction,
	std::uint64_t time_extent
)
{
	if (fraction > self.get_fraction_count())
	{
		throw nb::index_error(
			"DetectorEventFractionation: fraction out of range"
		);
	}
	return self.get_fraction_begin(fraction, time_extent);
}

std::size_t hash(const detector_event_fractionation &self)
{
	return std::hash<std::size_t>()(self.get_fraction_count());
}

std::string to_repr(const detector_event_fractionation &self)
{
	return "DetectorEventFractionation(fraction_count=" +
		std::to_string(self.get_fraction_count()) + ")";
}

} // anonymous namespace

void bind_detector_event_fractionation(nb::module_ &m)
{
	nb::class_<detector_event_fractionation> fractionation(
		m,
		"DetectorEventFractionation"
	);

	fractionation
		.def(nb::init<std::size_t>(), nb::arg("fraction_count"))
		.def(nb::self == nb::self)
		.def(nb::self != nb::self)
		.def("__hash__", &hash)
		.def("__repr__", &to_repr)
		.def_prop_ro(
			"fraction_count",
			&detector_event_fractionation::get_fraction_count
		)
		.def(
			"get_fraction_begin", &get_fraction_begin,
			nb::arg("fraction"), nb::arg("time_extent")
		);

	fractionation.attr("max_fraction_count") =
		std::size_t(detector_event_fractionation::max_fraction_count);
}

} // namespace vitrio
