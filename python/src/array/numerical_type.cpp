// SPDX-License-Identifier: LGPL-2.1-or-later

#include "numerical_type.hpp"

#include <vitrio/array/numerical_type.hpp>

namespace vitrio
{

namespace nb = nanobind;

void bind_numerical_type(nb::module_ &m)
{
	nb::enum_<numerical_type>(m, "NumericalType")
		.value("boolean", numerical_type::boolean)
		.value("int8", numerical_type::int8)
		.value("uint8", numerical_type::uint8)
		.value("int16", numerical_type::int16)
		.value("uint16", numerical_type::uint16)
		.value("int32", numerical_type::int32)
		.value("uint32", numerical_type::uint32)
		.value("int64", numerical_type::int64)
		.value("uint64", numerical_type::uint64)
		.value("float16", numerical_type::float16)
		.value("float32", numerical_type::float32)
		.value("float64", numerical_type::float64)
		.value("complex_float16", numerical_type::complex_float16)
		.value("complex_float32", numerical_type::complex_float32)
		.value("complex_float64", numerical_type::complex_float64);
}

} // namespace vitrio
