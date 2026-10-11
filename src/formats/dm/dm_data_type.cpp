// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_data_type.hpp"

#include <vitrio/exceptions/unsupported_operation_error.hpp>

#include <string>

namespace vitrio
{
namespace dm
{

namespace
{

// The codes DigitalMicrograph states the data type of an image with, as its
// documentation names them.
namespace data_type
{

const std::int64_t signed_int16 = 1;
const std::int64_t real4 = 2;
const std::int64_t complex8 = 3;
const std::int64_t packed_complex = 5;
const std::int64_t unsigned_int8 = 6;
const std::int64_t signed_int32 = 7;
const std::int64_t rgb = 8;
const std::int64_t signed_int8 = 9;
const std::int64_t unsigned_int16 = 10;
const std::int64_t unsigned_int32 = 11;
const std::int64_t real8 = 12;
const std::int64_t complex16 = 13;
const std::int64_t binary = 14;
const std::int64_t rgba_uint8 = 23;

} // namespace data_type

} // anonymous namespace

numerical_type get_data_type(std::int64_t code)
{
	switch (code)
	{
	case data_type::signed_int8:
		return numerical_type::int8;
	case data_type::unsigned_int8:
	case data_type::binary:
		return numerical_type::uint8;
	case data_type::signed_int16:
		return numerical_type::int16;
	case data_type::unsigned_int16:
		return numerical_type::uint16;
	case data_type::signed_int32:
		return numerical_type::int32;
	case data_type::unsigned_int32:
		return numerical_type::uint32;
	case data_type::real4:
		return numerical_type::float32;
	case data_type::real8:
		return numerical_type::float64;
	case data_type::complex8:
		return numerical_type::complex_float32;
	case data_type::complex16:
		return numerical_type::complex_float64;

	case data_type::packed_complex:
		throw unsupported_operation_error(
			"dm::get_data_type: The image holds packed complex values, "
			"which are not read."
		);

	case data_type::rgb:
	case data_type::rgba_uint8:
		throw unsupported_operation_error(
			"dm::get_data_type: The image holds RGB values, which are not "
			"read."
		);

	default:
		throw unsupported_operation_error(
			"dm::get_data_type: The image states a data type that is not "
			"read: " + std::to_string(code) + "."
		);
	}
}

} // namespace dm
} // namespace vitrio
