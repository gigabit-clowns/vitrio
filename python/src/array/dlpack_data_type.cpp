// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dlpack_data_type.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

using code = nb::dlpack::dtype_code;
using equivalence = std::pair<numerical_type, nb::dlpack::dtype>;

constexpr
nb::dlpack::dtype make_data_type(code kind, std::uint8_t bits) noexcept
{
	return nb::dlpack::dtype{static_cast<std::uint8_t>(kind), bits, 1};
}

// A complex type counts the bits of its two components together.
const std::array<equivalence, 15> equivalences = {{
	{numerical_type::boolean, make_data_type(code::Bool, 8)},
	{numerical_type::int8, make_data_type(code::Int, 8)},
	{numerical_type::uint8, make_data_type(code::UInt, 8)},
	{numerical_type::int16, make_data_type(code::Int, 16)},
	{numerical_type::uint16, make_data_type(code::UInt, 16)},
	{numerical_type::int32, make_data_type(code::Int, 32)},
	{numerical_type::uint32, make_data_type(code::UInt, 32)},
	{numerical_type::int64, make_data_type(code::Int, 64)},
	{numerical_type::uint64, make_data_type(code::UInt, 64)},
	{numerical_type::float16, make_data_type(code::Float, 16)},
	{numerical_type::float32, make_data_type(code::Float, 32)},
	{numerical_type::float64, make_data_type(code::Float, 64)},
	{numerical_type::complex_float16, make_data_type(code::Complex, 32)},
	{numerical_type::complex_float32, make_data_type(code::Complex, 64)},
	{numerical_type::complex_float64, make_data_type(code::Complex, 128)}
}};

} // anonymous namespace

numerical_type to_numerical_type(nb::dlpack::dtype type)
{
	const auto ite = std::find_if(
		equivalences.begin(),
		equivalences.end(),
		[type] (const equivalence &item) { return item.second == type; }
	);
	if (ite == equivalences.end())
	{
		throw nb::type_error(
			"vitrio has no numerical type for the elements of the array."
		);
	}

	return ite->first;
}

nb::dlpack::dtype to_dlpack_data_type(numerical_type type)
{
	const auto ite = std::find_if(
		equivalences.begin(),
		equivalences.end(),
		[type] (const equivalence &item) { return item.first == type; }
	);
	if (ite == equivalences.end())
	{
		throw nb::type_error("The numerical type is unknown.");
	}

	return ite->second;
}

} // namespace vitrio
