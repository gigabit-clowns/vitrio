// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_data_type.hpp"

#include "zarr_json.hpp"

#include <cstring>

namespace vitrio
{
namespace zarr
{

namespace
{

struct named_type
{
	const char *name;
	numerical_type type;
};

// The kind and size of a typestring after its byte order, and the names of
// version 3, for every type an image may hold.
const named_type v2_types[] = {
	{"b1", numerical_type::boolean},
	{"i1", numerical_type::int8},
	{"u1", numerical_type::uint8},
	{"i2", numerical_type::int16},
	{"u2", numerical_type::uint16},
	{"i4", numerical_type::int32},
	{"u4", numerical_type::uint32},
	{"i8", numerical_type::int64},
	{"u8", numerical_type::uint64},
	{"f2", numerical_type::float16},
	{"f4", numerical_type::float32},
	{"f8", numerical_type::float64},
	{"c8", numerical_type::complex_float32},
	{"c16", numerical_type::complex_float64},
};

const named_type v3_types[] = {
	{"bool", numerical_type::boolean},
	{"int8", numerical_type::int8},
	{"uint8", numerical_type::uint8},
	{"int16", numerical_type::int16},
	{"uint16", numerical_type::uint16},
	{"int32", numerical_type::int32},
	{"uint32", numerical_type::uint32},
	{"int64", numerical_type::int64},
	{"uint64", numerical_type::uint64},
	{"float16", numerical_type::float16},
	{"float32", numerical_type::float32},
	{"float64", numerical_type::float64},
	{"complex64", numerical_type::complex_float32},
	{"complex128", numerical_type::complex_float64},
};

template <std::size_t N>
const named_type* find_type(
	const named_type (&types)[N],
	const std::string &name
) noexcept
{
	for (const auto &candidate : types)
	{
		if (name == candidate.name)
		{
			return &candidate;
		}
	}

	return nullptr;
}

} // anonymous namespace

zarr_data_type parse_v2_data_type(
	const nlohmann::json &dtype,
	const std::string &where
)
{
	if (dtype.is_array())
	{
		refuse_metadata(where, "Arrays of a structured type are not read.");
	}
	if (!dtype.is_string() || dtype.get_ref<const std::string&>().size() < 3)
	{
		reject_metadata(where, "The data type is no typestring.");
	}

	const auto &text = dtype.get_ref<const std::string&>();
	const auto *const found = find_type(v2_types, text.substr(1));
	if (found == nullptr)
	{
		refuse_metadata(
			where, "Arrays of the data type \"" + text + "\" are not read.");
	}

	const bool single_byte = get_size(found->type) == 1;
	switch (text[0])
	{
	case '<':
		return {found->type, byte_order::little_endian};
	case '>':
		return {found->type, byte_order::big_endian};
	case '|':
		if (single_byte)
		{
			return {found->type, get_system_byte_order()};
		}
		break;
	default:
		break;
	}

	reject_metadata(
		where, "The data type \"" + text + "\" states no byte order.");
}

zarr_data_type parse_v3_data_type(
	const nlohmann::json &data_type,
	byte_order order,
	const std::string &where
)
{
	const nlohmann::json *name = &data_type;
	if (data_type.is_object())
	{
		name = &get_member(data_type, "name", where);
	}
	if (!name->is_string())
	{
		reject_metadata(where, "The data type is not named.");
	}

	const auto &text = name->get_ref<const std::string&>();
	const auto *const found = find_type(v3_types, text);
	if (found == nullptr)
	{
		refuse_metadata(
			where, "Arrays of the data type \"" + text + "\" are not read.");
	}

	return {found->type, order};
}

} // namespace zarr
} // namespace vitrio
