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
 * Each chunk the regions reach is read whole and decoded whole, and the
 * pieces of the regions on it are then moved into the array. A chunk with no
 * file, or that its shard does not store, is what the array is filled with.
 * Of a shard, its index is read first, then the chunks the regions reach, each
 * a read of its own.
 *
 * With asynchronous ports a file is opened in a task on the executor of
 * computations, since an open may wait on a file system that is far away,
 * which would stall every operation of the thread that drives the read. It is
 * then read through the port, and each chunk decoded and moved in a task
 * again. Without them, a read through a port would wait on that thread, so a
 * single task opens, reads, decodes and moves all of a file, and the files
 * are read on the threads of the executor at once.
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
	 * @param asynchronous Whether the transfers go through asynchronous
	 * ports.
	 * @param byte_budget The bytes of chunks it holds at once.
	 * @param chunks_in_flight The files it has open at once.
	 */
	zarr_read_operation(
		std::string directory,
		std::shared_ptr<const zarr_array_layout> layout,
		array destination,
		image_chunk_regions chunks,
		bool asynchronous,
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

	enum class step_kind : std::uint64_t
	{
		open,
		decode,
		whole,
		read_index,
		read_chunk
	};

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
		std::size_t reads = 0;
		std::size_t undecoded = 0;
	};

	void admit(file_transfer_scope &scope);
	void run(
		file_transfer_scope &scope,
		std::size_t id,
		step_kind kind
	);
	void queue_read(
		file_transfer_scope &scope,
		unit &file,
		std::uint64_t offset,
		std::vector<byte> &buffer,
		std::size_t id,
		step_kind kind
	);
	void check_read(
		const file_transfer_completion &completion,
		const unit &file,
		std::size_t expected
	) const;
	void close_after_read(unit &file);
	void finish_part(file_transfer_scope &scope, std::size_t part);
	void place_chunks(file_transfer_scope &scope, std::size_t unit);

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
	bool m_asynchronous;
	std::size_t m_byte_budget;
	std::size_t m_files_in_flight;
	std::size_t m_encoded_limit;

	// Written only by the task of each unit or part until the task
	// reports, and by the calls of the operation after it does.
	mutable std::vector<unit> m_units;
	mutable std::vector<part> m_parts;

	std::size_t m_next;
	std::size_t m_active;
	std::size_t m_reserved;
};

} // namespace zarr
} // namespace vitrio
