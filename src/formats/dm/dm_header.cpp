// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_header.hpp"

#include "dm_constants.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <cstring>
#include <string>

namespace vitrio
{
namespace dm
{

namespace
{

// Offsets of the fields of the header, of which only the first does not
// depend on the version.
const std::size_t version_offset = 0;
const std::size_t dm3_byte_order_offset = 8;
const std::size_t dm4_byte_order_offset = 12;

// The values of the byte order field.
const std::uint32_t big_endian_flag = 0;
const std::uint32_t little_endian_flag = 1;

std::uint32_t read_uint32(span<const byte> bytes, std::size_t offset) noexcept
{
	std::uint32_t value;
	std::memcpy(&value, bytes.data() + offset, sizeof(value));
	return convert_byte_order(
		value,
		byte_order::big_endian,
		get_system_byte_order()
	);
}

bool is_supported_version(std::uint32_t version) noexcept
{
	return version == 3 || version == 4;
}

bool is_byte_order_flag(std::uint32_t flag) noexcept
{
	return flag == big_endian_flag || flag == little_endian_flag;
}

std::size_t get_header_size(std::uint32_t version) noexcept
{
	return version == 3 ? dm3_header_size : dm4_header_size;
}

std::size_t get_byte_order_offset(std::uint32_t version) noexcept
{
	return version == 3 ? dm3_byte_order_offset : dm4_byte_order_offset;
}

bool is_flag(byte value) noexcept
{
	return value == 0 || value == 1;
}

} // anonymous namespace

dm_header::dm_header(std::uint32_t version, byte_order order) noexcept
	: m_version(version)
	, m_byte_order(order)
{
}

std::uint32_t dm_header::get_version() const noexcept
{
	return m_version;
}

byte_order dm_header::get_byte_order() const noexcept
{
	return m_byte_order;
}

std::size_t dm_header::get_word_size() const noexcept
{
	return m_version == 3 ? sizeof(std::uint32_t) : sizeof(std::uint64_t);
}

std::size_t dm_header::get_root_offset() const noexcept
{
	return get_header_size(m_version);
}

bool has_dm_signature(span<const byte> bytes) noexcept
{
	if (bytes.size() < sizeof(std::uint32_t))
	{
		return false;
	}

	const auto version = read_uint32(bytes, version_offset);
	if (!is_supported_version(version))
	{
		return false;
	}

	const auto root_offset = get_header_size(version);
	if (bytes.size() < root_offset + group_flags_size)
	{
		return false;
	}

	return is_byte_order_flag(
			read_uint32(bytes, get_byte_order_offset(version))
		) &&
		is_flag(bytes[root_offset]) &&
		is_flag(bytes[root_offset + 1]);
}

dm_header parse_header(span<const byte> bytes)
{
	if (bytes.size() < sizeof(std::uint32_t))
	{
		throw image_file_format_error(
			"dm::parse_header: The file is too short to hold a header."
		);
	}

	const auto version = read_uint32(bytes, version_offset);
	if (!is_supported_version(version))
	{
		throw image_file_format_error(
			"dm::parse_header: The file is of version " +
			std::to_string(version) + ", where only 3 and 4 are read."
		);
	}

	if (bytes.size() < get_header_size(version))
	{
		throw image_file_format_error(
			"dm::parse_header: The file is too short to hold a header."
		);
	}

	const auto flag = read_uint32(bytes, get_byte_order_offset(version));
	if (!is_byte_order_flag(flag))
	{
		throw image_file_format_error(
			"dm::parse_header: The file states a byte order that is neither "
			"big nor little endian."
		);
	}

	return dm_header(
		version,
		flag == little_endian_flag
			? byte_order::little_endian
			: byte_order::big_endian
	);
}

} // namespace dm
} // namespace vitrio
