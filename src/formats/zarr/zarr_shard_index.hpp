// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_shard_layout.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief Where one chunk lies in its shard.
 */
struct zarr_shard_entry
{
	/** @brief Whether the shard stores the chunk at all. */
	bool stored;

	/** @brief The offset of its bytes in the file of the shard. */
	std::uint64_t offset;

	/** @brief How many bytes it takes. */
	std::uint64_t size;
};

/**
 * @brief The index of one shard of a ZARR array, read and checked.
 */
class zarr_shard_index
{
public:
	/**
	 * @brief Read the index of a shard.
	 *
	 * @param encoded The bytes of the index, its checksum included.
	 * @param layout How the shards of the array are laid out.
	 * @param file_size How many bytes the file of the shard holds.
	 * @param path The file of the shard, which messages start with.
	 * @throws image_file_format_error If the index does not take the bytes
	 * the layout says, its checksum does not match, an entry is stored but
	 * one of its integers is all ones, or a chunk lies beyond the file.
	 */
	zarr_shard_index(
		span<const byte> encoded,
		const zarr_shard_layout &layout,
		std::uint64_t file_size,
		const std::string &path
	);

	/**
	 * @brief Get where one chunk of the shard lies.
	 *
	 * @param chunk Its position among the chunks of the shard, in C order.
	 * Must be below the count of chunks of a shard.
	 * @return zarr_shard_entry Where it lies.
	 */
	zarr_shard_entry get_entry(std::size_t chunk) const noexcept;

private:
	std::vector<std::uint64_t> m_entries;
};

} // namespace zarr
} // namespace vitrio
