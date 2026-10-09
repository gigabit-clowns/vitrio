// SPDX-License-Identifier: LGPL-2.1-or-later

#include "index_table.hpp"

#include "index_tuple.hpp"

#include <array/host_ndarray.hpp>

#include <vitrio/byte.hpp>
#include <vitrio/index_table.hpp>
#include <vitrio/span.hpp>

#include <nanobind/ndarray.h>
#include <nanobind/stl/vector.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

using contiguous_index_ndarray = nb::ndarray<
	std::size_t,
	nb::ndim<2>,
	nb::c_contig,
	nb::device::cpu
>;

void add(index_table &self, const std::vector<std::size_t> &index)
{
	self.add(make_span(index));
}

index_tuple get(const index_table &self, std::size_t position)
{
	if (position >= self.get_index_count())
	{
		throw nb::index_error("IndexTable index out of range");
	}

	return to_tuple(self.get(position));
}

template <typename T>
T read(const byte *element) noexcept
{
	T value;
	std::memcpy(&value, element, sizeof(T));
	return value;
}

std::int64_t read_signed(const byte *element, std::size_t size) noexcept
{
	switch (size)
	{
	case sizeof(std::int8_t):
		return read<std::int8_t>(element);
	case sizeof(std::int16_t):
		return read<std::int16_t>(element);
	case sizeof(std::int32_t):
		return read<std::int32_t>(element);
	default:
		return read<std::int64_t>(element);
	}
}

std::uint64_t read_unsigned(const byte *element, std::size_t size) noexcept
{
	switch (size)
	{
	case sizeof(std::uint8_t):
		return read<std::uint8_t>(element);
	case sizeof(std::uint16_t):
		return read<std::uint16_t>(element);
	case sizeof(std::uint32_t):
		return read<std::uint32_t>(element);
	default:
		return read<std::uint64_t>(element);
	}
}

// The indices of a table are unsigned, so a negative one would become a
// huge index on its way in if it were not refused.
std::size_t read_coordinate(
	const byte *element,
	std::size_t size,
	bool is_signed
)
{
	if (!is_signed)
	{
		return static_cast<std::size_t>(read_unsigned(element, size));
	}

	const auto value = read_signed(element, size);
	if (value < 0)
	{
		throw nb::value_error(
			"IndexTable.from_array takes no negative index"
		);
	}

	return static_cast<std::size_t>(value);
}

index_table from_array(const const_host_ndarray &indices)
{
	const auto data_type = indices.dtype();
	const auto kind = static_cast<nb::dlpack::dtype_code>(data_type.code);
	const auto is_signed = kind == nb::dlpack::dtype_code::Int;
	const auto is_unsigned = kind == nb::dlpack::dtype_code::UInt;
	if ((!is_signed && !is_unsigned) || data_type.lanes != 1)
	{
		throw nb::type_error(
			"IndexTable.from_array takes an array of integers"
		);
	}

	if (indices.ndim() != 2)
	{
		throw nb::value_error(
			"IndexTable.from_array takes an array of two dimensions, one "
			"row per index"
		);
	}

	const auto count = indices.shape(0);
	const auto rank = indices.shape(1);
	const auto size = indices.itemsize();
	const auto *origin = static_cast<const byte*>(indices.data());

	index_table result(rank);
	result.reserve(count);

	std::vector<std::size_t> index(rank);
	for (std::size_t row = 0; row < count; ++row)
	{
		for (std::size_t column = 0; column < rank; ++column)
		{
			const auto position =
				static_cast<std::int64_t>(row) * indices.stride(0) +
				static_cast<std::int64_t>(column) * indices.stride(1);
			const auto *element =
				origin + position * static_cast<std::int64_t>(size);
			index[column] = read_coordinate(element, size, is_signed);
		}

		result.add(make_span(index));
	}

	return result;
}

// The array is a copy, in memory numpy allocates: a view would be left
// dangling by the next index added to the table.
nb::object to_array(const index_table &self, nb::handle dtype, nb::handle copy)
{
	if (copy.is(nb::handle(Py_False)))
	{
		throw nb::value_error(
			"An IndexTable cannot become an array without a copy"
		);
	}

	const auto count = self.get_index_count();
	const auto rank = self.get_rank();

	const auto numpy = nb::module_::import_("numpy");
	const auto result = numpy.attr("empty")(
		nb::make_tuple(count, rank),
		numpy.attr("uintp")
	);

	const auto values = nb::cast<contiguous_index_ndarray>(result);
	for (std::size_t position = 0; position < count; ++position)
	{
		const auto index = self.get(position);
		std::copy(
			index.begin(),
			index.end(),
			values.data() + position * rank
		);
	}

	if (dtype.is_none())
	{
		return result;
	}

	return result.attr("astype")(dtype);
}

} // anonymous namespace

void bind_index_table(nb::module_ &m)
{
	nb::class_<index_table>(m, "IndexTable")
		.def(nb::init<>())
		.def(nb::init<std::size_t>(), nb::arg("rank"))
		.def_static(
			"from_array", &from_array,
			nb::arg("indices").noconvert()
		)
		.def("add", &add, nb::arg("index"))
		.def("clear", &index_table::clear)
		.def("reserve", &index_table::reserve, nb::arg("count"))
		.def_prop_ro("rank", &index_table::get_rank)
		.def("__len__", &index_table::get_index_count)
		.def("__getitem__", &get, nb::arg("position"))
		.def(
			"__array__", &to_array,
			nb::arg("dtype") = nb::none(), nb::arg("copy") = nb::none()
		);
}

} // namespace vitrio
