// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/span.hpp>

#include <memory/byte_order.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief Where a shard keeps its index, among its bytes.
 */
enum class zarr_index_location
{
	start,
	end
};

/**
 * @brief How a sharded ZARR array groups its chunks into files.
 *
 * The sharding codec of version 3 stores a block of chunks, a shard, as one
 * file: the chunks it holds, each encoded on its own, in any order, and an
 * index that says where each lies. The index is a pair of integers of 64 bits
 * per chunk, its offset in the file and its size in bytes, in C order over
 * the chunks of the shard, both all ones for a chunk that is not stored. The
 * shards tile the array as a grid of chunks would, and the chunks tile each
 * shard, so a chunk is placed in the array as if there were no shards.
 */
class zarr_shard_layout
{
public:
	/**
	 * @brief Lay the shards of an array out.
	 *
	 * @param shard_extents Extents of a shard, along the axes of the array.
	 * @param chunk_extents Extents of a chunk, which divide those of a shard.
	 * @param location Where the index is.
	 * @param index_order The order of the bytes of its integers.
	 * @param index_checksum Whether a CRC-32C of the index follows it.
	 * @throws std::invalid_argument If the extents do not have one rank, or
	 * those of a chunk do not divide those of a shard.
	 * @throws std::overflow_error If the bytes of the index can not be
	 * counted.
	 */
	zarr_shard_layout(
		std::vector<std::size_t> shard_extents,
		span<const std::size_t> chunk_extents,
		zarr_index_location location,
		byte_order index_order,
		bool index_checksum
	);

	/**
	 * @brief Get the extents of a shard.
	 *
	 * @return span<const std::size_t> The extents, along the axes of the
	 * array. It refers to storage owned by this layout.
	 */
	span<const std::size_t> get_shard_extents() const noexcept;

	/**
	 * @brief Get how many chunks a shard holds along each axis.
	 *
	 * @return span<const std::size_t> The counts. It refers to storage
	 * owned by this layout.
	 */
	span<const std::size_t> get_chunks_per_shard() const noexcept;

	/**
	 * @brief Get how many chunks a shard holds.
	 *
	 * @return std::size_t The product of @ref get_chunks_per_shard.
	 */
	std::size_t get_chunk_count() const noexcept;

	/**
	 * @brief Get where a shard keeps its index.
	 *
	 * @return zarr_index_location The location.
	 */
	zarr_index_location get_index_location() const noexcept;

	/**
	 * @brief Get the order of the bytes of the integers of the index.
	 *
	 * @return byte_order The order.
	 */
	byte_order get_index_byte_order() const noexcept;

	/**
	 * @brief Check whether a CRC-32C follows the index.
	 *
	 * @return bool Whether it does.
	 */
	bool has_index_checksum() const noexcept;

	/**
	 * @brief Get how many bytes the index takes, its checksum included.
	 *
	 * @return std::size_t The size in bytes.
	 */
	std::size_t get_index_byte_size() const noexcept;

	/**
	 * @brief Get the shard a chunk lies in.
	 *
	 * @param chunk The position of the chunk in the grid of chunks of the
	 * array.
	 * @param shard Where the position of the shard in the grid of shards is
	 * written.
	 */
	void get_shard(
		span<const std::size_t> chunk,
		std::vector<std::size_t> &shard
	) const;

	/**
	 * @brief Get where in the index of its shard a chunk is.
	 *
	 * @param chunk The position of the chunk in the grid of chunks of the
	 * array.
	 * @return std::size_t Its position among the chunks of its shard, in C
	 * order.
	 */
	std::size_t get_index_entry(span<const std::size_t> chunk) const noexcept;

private:
	std::vector<std::size_t> m_shard_extents;
	std::vector<std::size_t> m_chunks_per_shard;
	std::size_t m_chunk_count;
	zarr_index_location m_location;
	byte_order m_index_order;
	bool m_index_checksum;
	std::size_t m_index_byte_size;
};

} // namespace zarr
} // namespace vitrio
