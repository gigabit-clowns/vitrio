// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "numerical_type_dispatch.hpp"

#include "fixed_width_float.hpp"
#include "type_tag.hpp"

#include <complex>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace vitrio
{

template <typename F>
inline auto dispatch_numerical_type(F &&visitor, numerical_type type)
{
	switch (type)
	{
	case numerical_type::boolean:
		return std::forward<F>(visitor)(type_tag<bool>());
	case numerical_type::int8:
		return std::forward<F>(visitor)(type_tag<std::int8_t>());
	case numerical_type::uint8:
		return std::forward<F>(visitor)(type_tag<std::uint8_t>());
	case numerical_type::int16:
		return std::forward<F>(visitor)(type_tag<std::int16_t>());
	case numerical_type::uint16:
		return std::forward<F>(visitor)(type_tag<std::uint16_t>());
	case numerical_type::int32:
		return std::forward<F>(visitor)(type_tag<std::int32_t>());
	case numerical_type::uint32:
		return std::forward<F>(visitor)(type_tag<std::uint32_t>());
	case numerical_type::int64:
		return std::forward<F>(visitor)(type_tag<std::int64_t>());
	case numerical_type::uint64:
		return std::forward<F>(visitor)(type_tag<std::uint64_t>());
	case numerical_type::float16:
		return std::forward<F>(visitor)(type_tag<float16_t>());
	case numerical_type::float32:
		return std::forward<F>(visitor)(type_tag<float32_t>());
	case numerical_type::float64:
		return std::forward<F>(visitor)(type_tag<float64_t>());
	case numerical_type::complex_float16:
		return std::forward<F>(visitor)(type_tag<std::complex<float16_t>>());
	case numerical_type::complex_float32:
		return std::forward<F>(visitor)(type_tag<std::complex<float32_t>>());
	case numerical_type::complex_float64:
		return std::forward<F>(visitor)(type_tag<std::complex<float64_t>>());
	default:
		throw std::invalid_argument(
			"dispatch_numerical_type: The numerical type is unknown."
		);
	}
}

} // namespace vitrio
