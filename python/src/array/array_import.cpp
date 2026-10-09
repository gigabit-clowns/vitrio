// SPDX-License-Identifier: LGPL-2.1-or-later

#include "array_import.hpp"

#include "dlpack_data_type.hpp"

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

bool has_elements(const const_host_ndarray &source) noexcept
{
	for (std::size_t axis = 0; axis < source.ndim(); ++axis)
	{
		if (source.shape(axis) == 0)
		{
			return false;
		}
	}

	return true;
}

// Python gives the element at the origin and strides that may be negative,
// so elements may lie on either side of it. An array counts from the first
// byte of its memory instead, which is where the lowest element is.
std::size_t count_elements_before_origin(const const_host_ndarray &source)
{
	if (!has_elements(source))
	{
		return 0;
	}

	std::size_t count = 0;
	for (std::size_t axis = 0; axis < source.ndim(); ++axis)
	{
		const auto stride = source.stride(axis);
		if (stride < 0)
		{
			const auto steps = source.shape(axis) - 1;
			count += steps * static_cast<std::size_t>(-stride);
		}
	}

	return count;
}

std::size_t count_elements_after_origin(const const_host_ndarray &source)
{
	std::size_t count = 0;
	for (std::size_t axis = 0; axis < source.ndim(); ++axis)
	{
		const auto stride = source.stride(axis);
		if (stride > 0)
		{
			const auto steps = source.shape(axis) - 1;
			count += steps * static_cast<std::size_t>(stride);
		}
	}

	return count;
}

array_descriptor make_descriptor(const const_host_ndarray &source)
{
	std::vector<std::size_t> extents(source.ndim());
	std::vector<std::ptrdiff_t> strides(source.ndim());
	for (std::size_t axis = 0; axis < source.ndim(); ++axis)
	{
		extents[axis] = source.shape(axis);
		strides[axis] = static_cast<std::ptrdiff_t>(source.stride(axis));
	}

	return array_descriptor(
		std::move(extents),
		std::move(strides),
		static_cast<std::ptrdiff_t>(count_elements_before_origin(source)),
		to_numerical_type(source.dtype())
	);
}

// Size of the memory between the lowest and the highest element, in bytes.
std::size_t compute_memory_size(
	const const_host_ndarray &source,
	const array_descriptor &descriptor
)
{
	if (!has_elements(source))
	{
		return 0;
	}

	const auto count =
		static_cast<std::size_t>(descriptor.get_offset()) +
		count_elements_after_origin(source) +
		1;
	return count * get_size(descriptor.get_data_type());
}

// Distance from the first byte of the memory to the element at the origin,
// in bytes.
std::size_t compute_origin_position(const array_descriptor &descriptor)
{
	const auto offset = static_cast<std::size_t>(descriptor.get_offset());
	return offset * get_size(descriptor.get_data_type());
}

// DLPack has two capsules, and only the newer one says whether the memory
// may be written to. A library that hands out the older one for memory it
// does not want written says so through the buffer protocol alone, as JAX
// does. So what also hands out a buffer is asked whether it is read-only.
void require_writable(const host_ndarray &source)
{
	const auto exporter = nb::cast(source, nb::rv_policy::reference);

	Py_buffer view;
	if (PyObject_GetBuffer(exporter.ptr(), &view, PyBUF_FULL_RO) != 0)
	{
		PyErr_Clear();
		return;
	}

	const auto is_read_only = view.readonly != 0;
	PyBuffer_Release(&view);

	if (is_read_only)
	{
		throw nb::type_error(
			"The array hands out memory that cannot be written to."
		);
	}
}

} // anonymous namespace

array import_array(const host_ndarray &source, const memory_loan &loan)
{
	require_writable(source);

	const const_host_ndarray elements(source);
	auto descriptor = make_descriptor(elements);

	auto *origin = static_cast<byte*>(source.data());
	const span<byte> memory(
		origin - compute_origin_position(descriptor),
		compute_memory_size(elements, descriptor)
	);

	return array(memory, loan.get_owner(), std::move(descriptor));
}

const_array import_const_array(
	const const_host_ndarray &source,
	const memory_loan &loan
)
{
	auto descriptor = make_descriptor(source);

	const auto *origin = static_cast<const byte*>(source.data());
	const span<const byte> memory(
		origin - compute_origin_position(descriptor),
		compute_memory_size(source, descriptor)
	);

	return const_array(memory, loan.get_owner(), std::move(descriptor));
}

} // namespace vitrio
