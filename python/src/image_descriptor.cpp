// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_descriptor.hpp"

#include "index_tuple.hpp"

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/span.hpp>

#include <nanobind/operators.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <cstddef>
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
	image_descriptor *self,
	const std::vector<std::size_t> &extents,
	std::size_t core_rank,
	numerical_type data_type
)
{
	new (self) image_descriptor(make_span(extents), core_rank, data_type);
}

index_tuple get_extents(const image_descriptor &self)
{
	return to_tuple(self.get_extents());
}

index_tuple py_get_core_extents(const image_descriptor &descriptor)
{
	return to_tuple(get_core_extents(descriptor));
}

std::string to_repr(nb::handle self)
{
	std::ostringstream oss;
	oss << "ImageDescriptor(extents=" << nb::repr(self.attr("extents")).c_str()
		<< ", core_rank=" << nb::repr(self.attr("core_rank")).c_str()
		<< ", data_type=" << nb::repr(self.attr("data_type")).c_str()
		<< ")";
	return oss.str();
}

} // anonymous namespace

void bind_image_descriptor(nb::module_ &m)
{
	nb::class_<image_descriptor>(
		m, "ImageDescriptor",
		"What an image file holds: its extents, core rank and data type.\n"
		"\n"
		"The extents are given slowest axis first. The last `core_rank` of "
		"them are the shape of one image or volume, and the ones before them "
		"stack those: an `ImageDescriptor((10, 64, 96), 2, \"float32\")` is a "
		"stack of ten images of 64 by 96. `query_descriptor` returns the "
		"descriptor of a file, and `write` takes the one a file is created "
		"with."
	)
		.def(
			"__init__", &construct,
			nb::arg("extents"), nb::arg("core_rank"), nb::arg("data_type"),
			"Describe what a file holds.\n"
			"\n"
			"Args:\n"
			"    extents: The extents of the file, slowest axis first.\n"
			"    core_rank: How many of the last extents are one image or "
			"volume.\n"
			"    data_type: The `NumericalType` of the elements."
		)
		.def(nb::self == nb::self)
		.def(nb::self != nb::self)
		.def("__hash__", &image_descriptor::hash)
		.def("__repr__", &to_repr)
		.def_prop_ro(
			"extents", &get_extents,
			"The extents of the whole file, slowest axis first, as a tuple."
		)
		.def_prop_ro(
			"core_rank", &image_descriptor::get_core_rank,
			"How many of the last extents are one image or volume."
		)
		.def_prop_ro(
			"data_type", &image_descriptor::get_data_type,
			"The `NumericalType` of the elements."
		);

	m.def(
		"get_core_extents", &py_get_core_extents,
		nb::arg("descriptor"),
		"Get the shape of one image or volume of a file.\n"
		"\n"
		"These are the last `core_rank` extents of the descriptor, which "
		"leaves out the axis a stack stacks along. It is the shape the "
		"destination of a batch carries behind its leading extent.\n"
		"\n"
		"Args:\n"
		"    descriptor: The `ImageDescriptor` of the file.\n"
		"\n"
		"Returns:\n"
		"    tuple: The extents of one image or volume."
	);
}

} // namespace vitrio
