// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_tag_values.hpp"

#include "dm_constants.hpp"
#include "dm_encoding.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <assert.hpp>
#include <memory/reinterpret_bits.hpp>

#include <cstddef>
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

// Where the value of a datum lies in a file.
const byte* get_payload(const dm_tag &tag, span<const byte> file) noexcept
{
	VITRIO_ASSERT(tag.get_payload_offset() <= file.size());
	VITRIO_ASSERT(
		tag.get_payload_size() <= file.size() - tag.get_payload_offset()
	);

	return file.data() + tag.get_payload_offset();
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

	const auto *bytes = get_payload(tag, file);

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

bool holds_real(const dm_tag &tag) noexcept
{
	const auto encoding = tag.get_encoding();
	return
		!tag.is_group() &&
		encoding.size() == 1 &&
		(encoding[0] == encoding::float32 ||
			encoding[0] == encoding::float64);
}

double read_real(
	const dm_tag &tag,
	span<const byte> file,
	byte_order order
)
{
	if (!holds_real(tag))
	{
		throw image_file_format_error(
			"dm::read_real: The entry " + tag.get_label() +
			" does not hold a single real."
		);
	}

	// A real is read as the integer of its bits, which is what has a byte
	// order to convert.
	const auto *bytes = get_payload(tag, file);
	if (tag.get_encoding()[0] == encoding::float32)
	{
		return reinterpret_bits<float>(
			read_scalar<std::uint32_t>(bytes, order));
	}

	return reinterpret_bits<double>(read_scalar<std::uint64_t>(bytes, order));
}

bool holds_text(const dm_tag &tag) noexcept
{
	const auto encoding = tag.get_encoding();
	return
		!tag.is_group() &&
		encoding.size() == 3 &&
		encoding[0] == encoding::array &&
		encoding[1] == encoding::uint16;
}

std::u16string read_text(
	const dm_tag &tag,
	span<const byte> file,
	byte_order order
)
{
	if (!holds_text(tag))
	{
		throw image_file_format_error(
			"dm::read_text: The entry " + tag.get_label() +
			" does not hold a text."
		);
	}

	// The parser made the size of the value that of as many characters as
	// the encoding states.
	const auto *bytes = get_payload(tag, file);
	const auto length = tag.get_payload_size() / sizeof(char16_t);

	std::u16string text;
	text.reserve(length);
	for (std::size_t i = 0; i < length; ++i)
	{
		text.push_back(static_cast<char16_t>(read_scalar<std::uint16_t>(
			bytes + i * sizeof(char16_t), order)));
	}

	return text;
}

} // namespace dm
} // namespace vitrio
