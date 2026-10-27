// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_shard_layout.hpp"

#include <array/checked_arithmetic.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace vitrio
{
namespace zarr
{

namespace
{

// The offset and the size of each chunk, of 8 bytes each, and the checksum.
const std::ptrdiff_t entry_size = 16;
const std::size_t checksum_size = 4;

[[noreturn]]
void overflow()
{
	throw std::overflow_error(
		"zarr_shard_layout: The bytes of the index of a shard can not be "
		"counted."
	);
}

} // anonymous namespace

zarr_shard_layout::zarr_shard_layout(
	std::vector<std::size_t> shard_extents,
	span<const std::size_t> chunk_extents,
	zarr_index_location location,
	byte_order index_order,
	bool index_checksum
)
	: m_shard_extents(std::move(shard_extents))
	, m_chunks_per_shard(m_shard_extents.size())
	, m_chunk_count(1)
	, m_location(location)
	, m_index_order(index_order)
	, m_index_checksum(index_checksum)
	, m_index_byte_size(0)
{
	if (chunk_extents.size() != m_shard_extents.size())
	{
		throw std::invalid_argument(
			"zarr_shard_layout: The chunks do not have as many axes as the "
			"shards."
		);
	}

	std::ptrdiff_t count = 1;
	for (std::size_t axis = 0; axis < m_shard_extents.size(); ++axis)
	{
		if (
			chunk_extents[axis] == 0 ||
			m_shard_extents[axis] % chunk_extents[axis] != 0
		)
		{
			throw std::invalid_argument(
				"zarr_shard_layout: The chunks do not divide a shard."
			);
		}

		m_chunks_per_shard[axis] = m_shard_extents[axis] / chunk_extents[axis];
		if (
			m_chunks_per_shard[axis] > static_cast<std::size_t>(
				std::numeric_limits<std::ptrdiff_t>::max()) ||
			!checked_multiply(
				count, static_cast<std::ptrdiff_t>(m_chunks_per_shard[axis]),
				count)
		)
		{
			overflow();
		}
	}

	std::ptrdiff_t bytes;
	if (!checked_multiply(count, entry_size, bytes))
	{
		overflow();
	}
	if (index_checksum && !checked_add(
		bytes, static_cast<std::ptrdiff_t>(checksum_size), bytes))
	{
		overflow();
	}

	m_chunk_count = static_cast<std::size_t>(count);
	m_index_byte_size = static_cast<std::size_t>(bytes);
}

span<const std::size_t>
zarr_shard_layout::get_shard_extents() const noexcept
{
	return make_span(m_shard_extents);
}

span<const std::size_t>
zarr_shard_layout::get_chunks_per_shard() const noexcept
{
	return make_span(m_chunks_per_shard);
}

std::size_t zarr_shard_layout::get_chunk_count() const noexcept
{
	return m_chunk_count;
}

zarr_index_location zarr_shard_layout::get_index_location() const noexcept
{
	return m_location;
}

byte_order zarr_shard_layout::get_index_byte_order() const noexcept
{
	return m_index_order;
}

bool zarr_shard_layout::has_index_checksum() const noexcept
{
	return m_index_checksum;
}

std::size_t zarr_shard_layout::get_index_byte_size() const noexcept
{
	return m_index_byte_size;
}

void zarr_shard_layout::get_shard(
	span<const std::size_t> chunk,
	std::vector<std::size_t> &shard
) const
{
	shard.resize(chunk.size());
	for (std::size_t axis = 0; axis < chunk.size(); ++axis)
	{
		shard[axis] = chunk[axis] / m_chunks_per_shard[axis];
	}
}

std::size_t zarr_shard_layout::get_index_entry(
	span<const std::size_t> chunk
) const noexcept
{
	std::size_t entry = 0;
	for (std::size_t axis = 0; axis < chunk.size(); ++axis)
	{
		entry = entry * m_chunks_per_shard[axis] +
			chunk[axis] % m_chunks_per_shard[axis];
	}

	return entry;
}

} // namespace zarr
} // namespace vitrio
