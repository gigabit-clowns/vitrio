// SPDX-License-Identifier: LGPL-2.1-or-later

#include "dm_tag_parser.hpp"

#include "dm_constants.hpp"
#include "dm_encoding.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <memory/byte_order.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace vitrio
{
namespace dm
{

namespace
{

[[noreturn]] void throw_truncated()
{
	throw image_file_format_error(
		"dm::parse_tags: The tags of the file run past its end."
	);
}

// Reads the structure of a file from its first byte onwards, every read
// checked against its end. The structure is big-endian whatever the byte
// order of the values.
class tag_cursor
{
public:
	tag_cursor(span<const byte> file, std::size_t position, std::size_t word)
		noexcept
		: m_file(file)
		, m_position(position)
		, m_word_size(word)
	{
	}

	std::size_t get_position() const noexcept
	{
		return m_position;
	}

	std::size_t get_remaining() const noexcept
	{
		return m_file.size() - m_position;
	}

	void skip(std::size_t size)
	{
		if (size > get_remaining())
		{
			throw_truncated();
		}

		m_position += size;
	}

	const byte* take(std::size_t size)
	{
		const auto *first = m_file.data() + m_position;
		skip(size);
		return first;
	}

	std::uint8_t take_uint8()
	{
		return *take(1);
	}

	std::uint16_t take_uint16()
	{
		return take_big_endian<std::uint16_t>();
	}

	// A count or size of the structure, as wide as the version makes it.
	std::uint64_t take_word()
	{
		return m_word_size == sizeof(std::uint32_t)
			? take_big_endian<std::uint32_t>()
			: take_big_endian<std::uint64_t>();
	}

	std::size_t get_word_size() const noexcept
	{
		return m_word_size;
	}

private:
	template <typename T>
	T take_big_endian()
	{
		T value;
		std::memcpy(&value, take(sizeof(value)), sizeof(value));
		return convert_byte_order(
			value,
			byte_order::big_endian,
			get_system_byte_order()
		);
	}

	span<const byte> m_file;
	std::size_t m_position;
	std::size_t m_word_size;
};

bool is_version_4(const tag_cursor &cursor) noexcept
{
	return cursor.get_word_size() == sizeof(std::uint64_t);
}

// A count of things that each take at least a given number of bytes, refused
// when there is not room left for them. Without this, a count read from a
// damaged file would size a vector before anything else noticed.
std::size_t check_count(
	const tag_cursor &cursor,
	std::uint64_t count,
	std::size_t least_size
)
{
	if (count > cursor.get_remaining() / least_size)
	{
		throw_truncated();
	}

	return static_cast<std::size_t>(count);
}

void parse_entries(
	tag_cursor &cursor,
	dm_tag &group,
	std::size_t depth
);

dm_tag parse_data(tag_cursor &cursor, std::string label)
{
	const auto *delimiter = cursor.take(data_delimiter.size());
	if (!std::equal(
		data_delimiter.cbegin(),
		data_delimiter.cend(),
		reinterpret_cast<const char*>(delimiter)))
	{
		throw image_file_format_error(
			"dm::parse_tags: A datum of the file lacks its delimiter."
		);
	}

	const auto word_count = check_count(
		cursor,
		cursor.take_word(),
		cursor.get_word_size()
	);

	std::vector<std::uint64_t> encoding(word_count);
	for (auto &word : encoding)
	{
		word = cursor.take_word();
	}

	const auto size = get_encoded_size(
		make_span(encoding.data(), encoding.size())
	);
	const auto offset = cursor.get_position();
	cursor.skip(size);

	return dm_tag(std::move(label), std::move(encoding), offset, size);
}

dm_tag parse_group(tag_cursor &cursor, std::string label, std::size_t depth)
{
	if (depth > max_group_depth)
	{
		throw image_file_format_error(
			"dm::parse_tags: The groups of the file are nested too deep."
		);
	}

	dm_tag group(std::move(label));
	cursor.skip(group_flags_size);
	parse_entries(cursor, group, depth);

	return group;
}

void parse_entries(
	tag_cursor &cursor,
	dm_tag &group,
	std::size_t depth
)
{
	// Each entry takes its kind and the length of its label at least.
	const auto least_entry_size = sizeof(std::uint8_t) + sizeof(std::uint16_t);
	const auto count = check_count(
		cursor,
		cursor.take_word(),
		least_entry_size
	);

	for (std::size_t entry = 0; entry < count; ++entry)
	{
		const auto kind = cursor.take_uint8();
		const auto label_length = cursor.take_uint16();
		const auto *label = cursor.take(label_length);
		std::string text(reinterpret_cast<const char*>(label), label_length);

		if (is_version_4(cursor))
		{
			cursor.skip(cursor.get_word_size()); // The size of the entry.
		}

		switch (kind)
		{
		case tag_kind::group:
			group.add_child(parse_group(cursor, std::move(text), depth + 1));
			break;

		case tag_kind::data:
			group.add_child(parse_data(cursor, std::move(text)));
			break;

		default:
			throw image_file_format_error(
				"dm::parse_tags: An entry of the file is of a kind the "
				"format does not have: " + std::to_string(kind) + "."
			);
		}
	}
}

} // anonymous namespace

dm_tag parse_tags(span<const byte> file, const dm_header &header)
{
	if (header.get_root_offset() > file.size())
	{
		throw_truncated();
	}

	tag_cursor cursor(file, header.get_root_offset(), header.get_word_size());

	return parse_group(cursor, std::string(), 1);
}

} // namespace dm
} // namespace vitrio
