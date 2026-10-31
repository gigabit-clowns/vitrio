// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_array_layout.hpp"

#include <file_transfer/file_transfer_operation.hpp>
#include <strided_transfer/image_chunk_regions.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/byte.hpp>

#include <boost/interprocess/file_mapping.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief A read of regions of a ZARR array into an array, a file at a time.
 *
 * A file holds one chunk, or one shard of many chunks for a sharded array.
 * Each file the regions reach is read by a task of its own on the executor
 * of computations, which opens it and reads it whole, or of a shard its
 * index and then each chunk the regions reach, decodes each chunk whole and
 * moves the pieces of the regions on it into the array. A chunk with no
 * file, or that its shard does not store, is what the array is filled with.
 *
 * The files are read on the threads of the executor at once, and never
 * through a port of file transfers. A whole file is one read, which a port
 * gains nothing on, and what a port brings from the cache of the system it
 * copies on the one thread that drives it, which capped a read of cached
 * chunks at what one thread copies. An open may wait on a file system that
 * is far away, which a task keeps from the thread that drives the operation.
 *
 * It holds as many files at once as fit the budget of bytes, counting what
 * each chunk is read into and what it is decoded into, and as many as it may
 * have open, and one at least.
 */
class zarr_read_operation final
	: public file_transfer_operation
{
public:
	/**
	 * @brief Prepare a read.
	 *
	 * @param directory The directory of the array.
	 * @param layout How the array lays out its chunks.
	 * @param destination Where the regions land. It is kept until the read
	 * is done.
	 * @param chunks The regions, resolved into the chunks they reach. They
	 * must lie in the array and in @p destination.
	 * @param byte_budget The bytes of chunks it holds at once.
	 * @param chunks_in_flight The files it has open at once.
	 */
	zarr_read_operation(
		std::string directory,
		std::shared_ptr<const zarr_array_layout> layout,
		array destination,
		image_chunk_regions chunks,
		std::size_t byte_budget,
		std::size_t chunks_in_flight
	);

	~zarr_read_operation() override;

	void start(file_transfer_scope &scope) override;

	void on_transfer(
		const file_transfer_completion &completion,
		file_transfer_scope &scope
	) override;

	void on_task(
		std::uint64_t tag,
		std::exception_ptr error,
		file_transfer_scope &scope
	) override;

private:
	class step;

	// One chunk to decode: where its regions are, where it lies in its
	// file, and its bytes while they are held.
	struct part
	{
		std::size_t unit;
		std::size_t position;
		std::size_t entry;
		std::uint64_t offset = 0;
		std::vector<byte> encoded;
		bool missing = false;
	};

	// One file: a chunk, or a shard and the chunks of it to decode.
	struct unit
	{
		std::string path;
		std::unique_ptr<boost::interprocess::file_mapping> file;
		std::uint64_t size = 0;
		std::vector<std::size_t> parts;
		std::vector<byte> index;
		bool missing = false;
	};

	void admit(file_transfer_scope &scope);

	std::uint64_t get_index_offset(const unit &file) const;
	void locate_chunks(std::size_t unit) const;
	std::size_t get_unit_bytes(std::size_t unit) const;

	void open(std::size_t unit) const;
	void read_whole(std::size_t unit) const;
	void decode(std::size_t part) const;

	std::string m_directory;
	std::shared_ptr<const zarr_array_layout> m_layout;
	array m_destination;
	byte *m_array_data;
	image_chunk_regions m_chunks;
	std::size_t m_byte_budget;
	std::size_t m_files_in_flight;
	std::size_t m_encoded_limit;

	// Written only by the task of each unit until the task reports, and by
	// the calls of the operation after it does.
	mutable std::vector<unit> m_units;
	mutable std::vector<part> m_parts;

	std::size_t m_next;
	std::size_t m_active;
	std::size_t m_reserved;
};

} // namespace zarr
} // namespace vitrio
