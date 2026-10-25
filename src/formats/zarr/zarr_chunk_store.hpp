// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_array_layout.hpp"

#include <vitrio/array/const_array.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief The chunks of a ZARR array being written: those held in memory
 * while they are written, and the files of those that were stored.
 *
 * A chunk is written into memory of its own, laid out as it is stored, and
 * stored once every value of it in the array has been written, or when it is
 * pushed out to make room or flushed. Storing it encodes it and writes a file
 * beside the one of the chunk, which is then renamed over it, so a reader
 * never sees half a chunk. A chunk that holds nothing but the fill value is
 * not stored, and its file, if any, is removed, as the reader reads a chunk
 * that is not stored. A chunk that is written again once it has been stored
 * is read back into memory first, so that what of it is not written again
 * stays.
 *
 * Its methods may be called at once from many threads. Writes of one chunk
 * at once are carried out one after the other; writes that do not overlap
 * may be carried out at once.
 */
class zarr_chunk_store
{
public:
	/**
	 * @brief Prepare to write the chunks of an array.
	 *
	 * @param directory The directory of the array, whose metadata is
	 * written.
	 * @param layout How the array lays its chunks out. Not sharded.
	 * @param byte_budget The bytes of chunks held in memory past which those
	 * written least recently are stored.
	 */
	zarr_chunk_store(
		std::string directory,
		std::shared_ptr<const zarr_array_layout> layout,
		std::size_t byte_budget
	);

	zarr_chunk_store(const zarr_chunk_store &other) = delete;
	zarr_chunk_store& operator=(const zarr_chunk_store &other) = delete;
	~zarr_chunk_store();

	/**
	 * @brief Get the directory of the array.
	 *
	 * @return const std::string& The directory.
	 */
	const std::string& get_directory() const noexcept;

	/**
	 * @brief Get how the array lays its chunks out.
	 *
	 * @return const zarr_array_layout& The layout.
	 */
	const zarr_array_layout& get_layout() const noexcept;

	/**
	 * @brief Write pieces of an array into one chunk.
	 *
	 * @param chunk The position of the chunk in the grid.
	 * @param pieces The plans of the pieces, stated against the chunk and
	 * @p source, as image_chunk_regions resolves them.
	 * @param source The values the pieces are taken from.
	 * @throws image_file_error If a file can not be read or written.
	 * @throws image_file_format_error If a chunk stored before is malformed.
	 * @throws unsupported_operation_error If the values can not be converted
	 * into the type of the array.
	 */
	void write(
		span<const std::size_t> chunk,
		const std::vector<const image_transfer_plan*> &pieces,
		const const_array &source
	);

	/**
	 * @brief Get the chunks held in memory.
	 *
	 * @return std::vector<std::vector<std::size_t>> Their positions.
	 */
	std::vector<std::vector<std::size_t>> get_held_chunks() const;

	/**
	 * @brief Store a chunk held in memory, if it still is.
	 *
	 * @param chunk The position of the chunk.
	 * @throws image_file_error If its file can not be written.
	 */
	void store(span<const std::size_t> chunk);

	/**
	 * @brief Take the files written since this was last called, which a
	 * flush syncs.
	 *
	 * @return std::vector<std::string> Their paths.
	 */
	std::vector<std::string> take_unsynced();

	/**
	 * @brief Note a file written beside the chunks, such as metadata, for
	 * the next flush to sync.
	 *
	 * @param path The file.
	 */
	void add_unsynced(const std::string &path);

	/**
	 * @brief Make what was written to a file reach the storage.
	 *
	 * @param path The file. One that is gone, as a chunk of the fill value
	 * stored since, is passed over.
	 * @throws image_file_error If the file can not be synced.
	 */
	static void sync(const std::string &path);

private:
	struct held_chunk
	{
		std::mutex mutex;
		std::vector<byte> values;
		bool ready = false;
		bool retired = false;
		std::size_t written = 0;
		std::size_t valid = 0;
		std::uint64_t touched = 0;
	};

	using chunk_key = std::vector<std::size_t>;

	std::shared_ptr<held_chunk> acquire(const chunk_key &chunk);
	void fill(const chunk_key &chunk, held_chunk &held);
	void store_locked(const chunk_key &chunk, held_chunk &held);
	void make_room();
	std::string get_path(const chunk_key &chunk) const;
	std::size_t count_valid(const chunk_key &chunk) const;

	std::string m_directory;
	std::shared_ptr<const zarr_array_layout> m_layout;
	std::size_t m_byte_budget;
	std::size_t m_chunk_bytes;
	std::vector<byte> m_fill_chunk;

	mutable std::mutex m_mutex;
	std::map<chunk_key, std::shared_ptr<held_chunk>> m_held;
	std::set<chunk_key> m_stored;
	std::set<std::string> m_unsynced;
	std::size_t m_held_bytes;
	std::uint64_t m_clock;
	std::uint64_t m_partials;
};

} // namespace zarr
} // namespace vitrio
