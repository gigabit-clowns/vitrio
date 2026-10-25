// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_shard_index.hpp"

#include "crc32c.hpp"

#include <vitrio/exceptions/image_file_format_error.hpp>

#include <assert.hpp>

#include <cstring>
#include <limits>

namespace vitrio
{
namespace zarr
{

namespace
{

const std::uint64_t not_stored = std::numeric_limits<std::uint64_t>::max();

[[noreturn]]
void reject(const std::string &path, const std::string &reason)
{
	throw image_file_format_error(path + ": zarr_shard_index: " + reason);
}

std::uint64_t read_integer(const byte *bytes, byte_order order) noexcept
{
	std::uint64_t value = 0;
	for (int i = 0; i < 8; ++i)
	{
		const auto position = order == byte_order::little_endian ? 7 - i : i;
		value = value << 8 | bytes[position];
	}

	return value;
}

} // anonymous namespace

zarr_shard_index::zarr_shard_index(
	span<const byte> encoded,
	const zarr_shard_layout &layout,
	std::uint64_t file_size,
	const std::string &path
)
	: m_entries(2 * layout.get_chunk_count())
{
	if (encoded.size() != layout.get_index_byte_size())
	{
		reject(path, "The index does not take the bytes its shard says.");
	}

	const auto size = m_entries.size() * 8;
	if (layout.has_index_checksum())
	{
		const auto *const stored = encoded.data() + size;
		const auto expected =
			static_cast<std::uint32_t>(stored[0]) |
			static_cast<std::uint32_t>(stored[1]) << 8 |
			static_cast<std::uint32_t>(stored[2]) << 16 |
			static_cast<std::uint32_t>(stored[3]) << 24;
		if (compute_crc32c(make_span(encoded.data(), size)) != expected)
		{
			reject(path, "The checksum of the index does not match it.");
		}
	}

	const auto order = layout.get_index_byte_order();
	for (std::size_t i = 0; i < m_entries.size(); ++i)
	{
		m_entries[i] = read_integer(encoded.data() + 8 * i, order);
	}

	for (std::size_t chunk = 0; chunk < layout.get_chunk_count(); ++chunk)
	{
		const auto offset = m_entries[2 * chunk];
		const auto bytes = m_entries[2 * chunk + 1];
		if (offset == not_stored && bytes == not_stored)
		{
			continue;
		}
		if (offset == not_stored || bytes == not_stored)
		{
			reject(path, "An entry of the index is half empty.");
		}
		if (offset > file_size || bytes > file_size - offset)
		{
			reject(path, "A chunk lies beyond the end of its shard.");
		}
	}
}

zarr_shard_entry zarr_shard_index::get_entry(std::size_t chunk) const noexcept
{
	VITRIO_ASSERT(2 * chunk + 1 < m_entries.size());

	const auto offset = m_entries[2 * chunk];
	if (offset == not_stored)
	{
		return {false, 0, 0};
	}

	return {true, offset, m_entries[2 * chunk + 1]};
}

} // namespace zarr
} // namespace vitrio
