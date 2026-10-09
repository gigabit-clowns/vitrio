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
	nb::class_<image_location> location(m, "ImageLocation");

	location
		.def(nb::init<>())
		.def(
			"__init__", &construct,
			nb::arg("key"),
			nb::arg("index_in_stack") =
				std::size_t(image_location::no_stack_index)
		)
		.def_static("from_string", &from_string, nb::arg("text"))
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
		.def_prop_ro("key", &image_location::get_key)
		.def_prop_ro(
			"index_in_stack",
			&image_location::get_index_in_stack
		)
		.def_prop_ro(
			"has_index_in_stack",
			&image_location::has_index_in_stack
		);

	location.attr("no_stack_index") =
		std::size_t(image_location::no_stack_index);
}

} // namespace vitrio
