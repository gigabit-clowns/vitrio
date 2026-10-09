// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/array/numerical_type.hpp>

#include <cstdint>

namespace vitrio
{

std::size_t get_size(numerical_type type) noexcept
{
	switch (type)
	{
	case numerical_type::boolean: return sizeof(bool);
	case numerical_type::int8: return sizeof(std::int8_t);
	case numerical_type::uint8: return sizeof(std::uint8_t);
	case numerical_type::int16: return sizeof(std::int16_t);
	case numerical_type::uint16: return sizeof(std::uint16_t);
	case numerical_type::int32: return sizeof(std::int32_t);
	case numerical_type::uint32: return sizeof(std::uint32_t);
	case numerical_type::int64: return sizeof(std::int64_t);
	case numerical_type::uint64: return sizeof(std::uint64_t);
	case numerical_type::float16: return 2;
	case numerical_type::float32: return 4;
	case numerical_type::float64: return 8;
	case numerical_type::complex_float16: return 4;
	case numerical_type::complex_float32: return 8;
	case numerical_type::complex_float64: return 16;
	default: return 0;
	}
}

const char* to_string(numerical_type type) noexcept
{
	switch (type)
	{
	case numerical_type::boolean: return "boolean";
	case numerical_type::int8: return "int8";
	case numerical_type::uint8: return "uint8";
	case numerical_type::int16: return "int16";
	case numerical_type::uint16: return "uint16";
	case numerical_type::int32: return "int32";
	case numerical_type::uint32: return "uint32";
	case numerical_type::int64: return "int64";
	case numerical_type::uint64: return "uint64";
	case numerical_type::float16: return "float16";
	case numerical_type::float32: return "float32";
	case numerical_type::float64: return "float64";
	case numerical_type::complex_float16: return "complex_float16";
	case numerical_type::complex_float32: return "complex_float32";
	case numerical_type::complex_float64: return "complex_float64";
	default: return "";
	}
}

std::ostream& operator<<(std::ostream &os, numerical_type type)
{
	return os << to_string(type);
}

} // namespace vitrio
