// SPDX-License-Identifier: LGPL-2.1-or-later

#include "array.hpp"

#include "dlpack_data_type.hpp"

#include <index_tuple.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>

#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

index_tuple get_shape(const array &self)
{
	return to_tuple(self.get_descriptor().get_extents());
}

numerical_type get_data_type(const array &self)
{
	return self.get_descriptor().get_data_type();
}

std::size_t get_length(const array &self)
{
	const auto extents = self.get_descriptor().get_extents();
	if (extents.empty())
	{
		throw nb::type_error("len() of an array with no dimensions");
	}

	return extents.front();
}

std::string to_repr(nb::handle self)
{
	std::ostringstream oss;
	oss << "Array(shape=" << nb::repr(self.attr("shape")).c_str()
		<< ", data_type=" << nb::repr(self.attr("data_type")).c_str()
		<< ")";
	return oss.str();
}

// Hands the memory out through the object nanobind exports arrays with,
// which implements DLPack and the buffer protocol. That object holds the
// Array, so the memory lives for as long as anything reads or writes it.
nb::object export_memory(nb::handle self)
{
	auto &exported = nb::cast<array&>(self);
	const auto &descriptor = exported.get_descriptor();
	const auto data_type = descriptor.get_data_type();
	const auto extents = descriptor.get_extents();
	const auto strides = descriptor.get_strides();

	const std::vector<std::size_t> shape(extents.begin(), extents.end());
	const std::vector<std::int64_t>
		element_strides(strides.begin(), strides.end());
	const auto origin =
		static_cast<std::size_t>(descriptor.get_offset()) *
		get_size(data_type);

	return nb::ndarray<nb::array_api>(
		exported.get_data(),
		shape.size(),
		shape.data(),
		self,
		element_strides.data(),
		to_dlpack_data_type(data_type),
		nb::device::cpu::value,
		0,
		'C',
		origin
	).cast();
}

nb::object to_dlpack(nb::handle self, const nb::kwargs &arguments)
{
	return export_memory(self).attr("__dlpack__")(**arguments);
}

nb::object get_dlpack_device(nb::handle self)
{
	return export_memory(self).attr("__dlpack_device__")();
}

// The view names the exported object as the one it came from, which is how
// the buffer protocol lets a request be answered by another object.
int get_buffer(PyObject *self, Py_buffer *view, int flags) noexcept
{
	view->obj = nullptr;

	try
	{
		const auto exported = export_memory(nb::handle(self));
		return PyObject_GetBuffer(exported.ptr(), view, flags);
	}
	catch (nb::python_error &error)
	{
		error.restore();
	}
	catch (const std::exception &error)
	{
		PyErr_SetString(PyExc_BufferError, error.what());
	}

	return -1;
}

const std::array<PyType_Slot, 2> buffer_slots = {{
	{Py_bf_getbuffer, reinterpret_cast<void*>(&get_buffer)},
	{0, nullptr}
}};

} // anonymous namespace

void bind_array(nb::module_ &m)
{
	nb::class_<array>(m, "Array", nb::type_slots(buffer_slots.data()))
		.def_prop_ro("shape", &get_shape)
		.def_prop_ro("data_type", &get_data_type)
		.def("__len__", &get_length)
		.def("__repr__", &to_repr)
		.def("__dlpack__", &to_dlpack)
		.def("__dlpack_device__", &get_dlpack_device);
}

} // namespace vitrio
