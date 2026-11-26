// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_metadata.hpp"

#include <vitrio/image_metadata_value.hpp>

#include <nanobind/stl/string.h>

#include <cstdint>
#include <string>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

image_metadata_value import_value(nb::handle value)
{
	// Before anything else: a boolean of Python is an integer of Python.
	if (nb::isinstance<nb::bool_>(value))
	{
		throw nb::type_error(
			"A value of the metadata is an integer, a real or a text, and "
			"a boolean is none of them."
		);
	}

	if (nb::isinstance<nb::str>(value))
	{
		return image_metadata_value(nb::cast<std::string>(value));
	}

	if (nb::isinstance<nb::float_>(value))
	{
		return image_metadata_value(nb::cast<double>(value));
	}

	// An int, or what stands for one, as an integer of numpy does.
	if (PyIndex_Check(value.ptr()))
	{
		const auto index = nb::steal(PyNumber_Index(value.ptr()));
		if (!index.is_valid())
		{
			throw nb::python_error();
		}

		const auto integer = PyLong_AsLongLong(index.ptr());
		if (integer == -1 && PyErr_Occurred())
		{
			throw nb::python_error();
		}

		return image_metadata_value(integer);
	}

	// What stands for a real, as a number of numpy of another width does.
	if (nb::hasattr(value, "__float__"))
	{
		const auto real = PyFloat_AsDouble(value.ptr());
		if (real == -1.0 && PyErr_Occurred())
		{
			throw nb::python_error();
		}

		return image_metadata_value(real);
	}

	throw nb::type_error(
		"A value of the metadata is an integer, a real or a text."
	);
}

// A text of a file is whatever bytes the file holds, and Python has a text
// as characters. It is read as UTF-8, which the texts of most files are,
// and what is not UTF-8 in it becomes the replacement character rather
// than a failure to read everything else the file states.
nb::object export_text(const std::string &text)
{
	auto result = nb::steal(PyUnicode_DecodeUTF8(
		text.data(),
		static_cast<Py_ssize_t>(text.size()),
		"replace"
	));
	if (!result.is_valid())
	{
		throw nb::python_error();
	}

	return result;
}

nb::object export_value(const image_metadata_value &value)
{
	switch (value.get_type())
	{
	case image_metadata_value_type::integer:
		return nb::int_(value.get_integer());
	case image_metadata_value_type::real:
		return nb::float_(value.get_real());
	default: // image_metadata_value_type::text
		return export_text(value.get_text());
	}
}

} // anonymous namespace

image_metadata import_image_metadata(nb::handle mapping)
{
	image_metadata result;
	if (mapping.is_none())
	{
		return result;
	}

	// A sequence passes for a mapping to Python, but has no items.
	if (!PyMapping_Check(mapping.ptr()) || !nb::hasattr(mapping, "items"))
	{
		throw nb::type_error(
			"The metadata is a mapping of labels to values."
		);
	}

	for (nb::handle item : mapping.attr("items")())
	{
		const nb::handle label = item[0];
		if (!nb::isinstance<nb::str>(label))
		{
			throw nb::type_error("A label of the metadata is a string.");
		}

		result.set(nb::cast<std::string>(label), import_value(item[1]));
	}

	return result;
}

nb::dict export_image_metadata(const image_metadata &metadata)
{
	nb::dict result;
	for (const auto &entry : metadata.get_entries())
	{
		result[export_text(entry.first)] = export_value(entry.second);
	}

	return result;
}

} // namespace vitrio
