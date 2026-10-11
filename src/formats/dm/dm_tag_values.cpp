// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_tag_values.hpp"

#include "dm_constants.hpp"
#include "dm_encoding.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <assert.hpp>

#include <cstring>
#include <limits>
#include <string>

namespace vitrio
{
namespace dm
{

namespace
{

template <typename T>
T read_scalar(const byte *bytes, byte_order order) noexcept
{
	T value;
	std::memcpy(&value, bytes, sizeof(value));
	return convert_byte_order(value, order, get_system_byte_order());
}

std::int64_t to_int64(std::uint64_t value, const dm_tag &tag)
{
	if (value > static_cast<std::uint64_t>(
		std::numeric_limits<std::int64_t>::max()))
	{
		throw image_file_format_error(
			"dm::read_integer: The datum " + tag.get_label() +
			" holds an integer too large to read."
		);
	}

	return static_cast<std::int64_t>(value);
}

} // anonymous namespace

std::int64_t read_integer(
	const dm_tag &tag,
	span<const byte> file,
	byte_order order
)
{
	const auto encoding = tag.get_encoding();
	if (tag.is_group() || encoding.size() != 1 || !is_integer(encoding[0]))
	{
		throw image_file_format_error(
			"dm::read_integer: The entry " + tag.get_label() +
			" does not hold a single integer."
		);
	}

	VITRIO_ASSERT(tag.get_payload_offset() <= file.size());
	VITRIO_ASSERT(
		tag.get_payload_size() <= file.size() - tag.get_payload_offset()
	);
	const auto *bytes = file.data() + tag.get_payload_offset();

	switch (encoding[0])
	{
	case encoding::int8:
		return read_scalar<std::int8_t>(bytes, order);
	case encoding::uint8:
		return read_scalar<std::uint8_t>(bytes, order);
	case encoding::int16:
		return read_scalar<std::int16_t>(bytes, order);
	case encoding::uint16:
		return read_scalar<std::uint16_t>(bytes, order);
	case encoding::int32:
		return read_scalar<std::int32_t>(bytes, order);
	case encoding::uint32:
		return read_scalar<std::uint32_t>(bytes, order);
	case encoding::int64:
		return read_scalar<std::int64_t>(bytes, order);
	default:
		VITRIO_ASSERT(encoding[0] == encoding::uint64);
		return to_int64(read_scalar<std::uint64_t>(bytes, order), tag);
	}
}

} // namespace dm
} // namespace vitrio
