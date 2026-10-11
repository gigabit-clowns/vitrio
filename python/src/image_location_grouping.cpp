// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_location_grouping.hpp"

#include "index_tuple.hpp"

#include <vitrio/image_location.hpp>
#include <vitrio/image_location_grouping.hpp>
#include <vitrio/span.hpp>

#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <cstddef>
#include <new>
#include <sstream>
#include <vector>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

using group = image_location_grouping::group;

void construct(
	image_location_grouping *self,
	const std::vector<image_location> &locations
)
{
	new (self) image_location_grouping(make_span(locations));
}

index_tuple get_indices(const group &self)
{
	return to_tuple(self.get_indices());
}

std::string to_repr(nb::handle self)
{
	std::ostringstream oss;
	oss << "ImageLocationGrouping.Group(key="
		<< nb::repr(self.attr("key")).c_str()
		<< ", indices=" << nb::repr(self.attr("indices")).c_str() << ")";
	return oss.str();
}

} // anonymous namespace

void bind_image_location_grouping(nb::module_ &m)
{
	nb::class_<image_location_grouping> grouping(m, "ImageLocationGrouping");

	// A group refers to what its grouping holds, so the grouping is kept
	// alive for as long as one of its groups is.
	grouping
		.def("__init__", &construct, nb::arg("locations"))
		.def("__len__", &image_location_grouping::get_group_count)
		.def(
			"__getitem__", &image_location_grouping::get_group,
			nb::arg("index"),
			nb::keep_alive<0, 1>()
		);

	nb::class_<group>(grouping, "Group")
		.def("__repr__", &to_repr)
		.def_prop_ro("key", &group::get_key)
		.def_prop_ro("is_whole", &group::is_whole)
		.def_prop_ro("indices", &get_indices);
}

} // namespace vitrio
