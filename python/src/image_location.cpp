// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_location.hpp"

#include "utf8_string.hpp"

#include <vitrio/image_location.hpp>

#include <nanobind/operators.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>

#include <cstddef>
#include <filesystem>
#include <new>
#include <sstream>
#include <string>
#include <tuple>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

using state = std::tuple<std::string, std::size_t>;

void construct(
	image_location *self,
	const std::filesystem::path &key,
	std::size_t index_in_stack
)
{
	new (self) image_location(to_utf8_string(key), index_in_stack);
}

image_location from_string(const std::string &text)
{
	image_location result;
	if (!parse_image_location(text, result))
	{
		std::ostringstream oss;
		oss << "\"" << text << "\" is not an image location. It is written "
			<< "\"index@key\", the index counting from one, or \"key\".";
		throw nb::value_error(oss.str().c_str());
	}

	return result;
}

std::string py_to_string(const image_location &self)
{
	return to_string(self);
}

std::string to_repr(nb::handle self)
{
	std::ostringstream oss;
	oss << "ImageLocation(key=" << nb::repr(self.attr("key")).c_str();
	if (nb::cast<bool>(self.attr("has_index_in_stack")))
	{
		oss << ", index_in_stack="
			<< nb::repr(self.attr("index_in_stack")).c_str();
	}
	oss << ")";
	return oss.str();
}

state get_state(const image_location &self)
{
	return state(self.get_key(), self.get_index_in_stack());
}

void set_state(image_location &self, const state &value)
{
	new (&self) image_location(std::get<0>(value), std::get<1>(value));
}

} // anonymous namespace

void bind_image_location(nb::module_ &m)
{
	nb::class_<image_location> location(
		m, "ImageLocation",
		"Names an image: a file and, for a stack, the index of one in it.\n"
		"\n"
		"A location without an index names the whole file. It is what the "
		"reads and writes of batches are given a list of, so that the images "
		"of a batch may come from any number of files."
	);
	location
		.def(nb::init<>(), "Make a location that names no file.")
		.def(
			"__init__", &construct,
			nb::arg("key"),
			nb::arg("index_in_stack") =
				std::size_t(image_location::no_stack_index),
			"Name an image.\n"
			"\n"
			"Args:\n"
			"    key: The file, as the path that opens it.\n"
			"    index_in_stack: The image of the stack, counting from zero. "
			"Defaults to naming the whole file."
		)
		.def_static(
			"from_string", &from_string, nb::arg("text"),
			"Parse the spelling a STAR file of RELION uses.\n"
			"\n"
			"It is written `\"index@key\"`, the index counting from one, or "
			"`\"key\"` for a whole file. "
			"`ImageLocation.from_string(\"3@stack.mrcs\")` is the third image "
			"of the stack, whose `index_in_stack` is 2.\n"
			"\n"
			"Args:\n"
			"    text: The location, as written.\n"
			"\n"
			"Returns:\n"
			"    ImageLocation: The location it names.\n"
			"\n"
			"Raises:\n"
			"    ValueError: If `text` is not a location."
		)
		.def(nb::self == nb::self)
		.def(nb::self != nb::self)
		.def(nb::self < nb::self)
		.def(nb::self <= nb::self)
		.def(nb::self > nb::self)
		.def(nb::self >= nb::self)
		.def("__hash__", &image_location::hash)
		.def("__str__", &py_to_string)
		.def("__repr__", &to_repr)
		.def("__getstate__", &get_state)
		.def("__setstate__", &set_state)
		.def_prop_ro(
			"key", &image_location::get_key,
			"The file, as the path that opens it."
		)
		.def_prop_ro(
			"index_in_stack", &image_location::get_index_in_stack,
			"The image of the stack, counting from zero.\n"
			"\n"
			"It is `ImageLocation.no_stack_index` for a location that names "
			"the whole file."
		)
		.def_prop_ro(
			"has_index_in_stack", &image_location::has_index_in_stack,
			"Whether the location names one image of a stack."
		);

	location.attr("no_stack_index") =
		std::size_t(image_location::no_stack_index);
}

} // namespace vitrio
