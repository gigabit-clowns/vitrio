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
 * @brief A read of regions of a ZARR array into an array, a chunk at a time.
 *
 * Each chunk the regions reach is a file of its own, read whole and decoded
 * whole, and the pieces of the regions on it are then moved into the array.
 * A chunk with no file is what the array is filled with.
 *
 * With asynchronous ports a chunk is opened in a task on the executor of
 * computations, since an open may wait on a file system that is far away,
 * which would stall every operation of the thread that drives the read.
 * Its file is then read through the port, and decoded and moved in a task
 * again. Without them, a read through a port would wait on that thread, so a
 * single task opens, reads, decodes and moves each chunk, and the chunks are
 * read on the threads of the executor at once.
 *
 * It holds as many chunks at once as fit the budget of bytes, counting what
 * a chunk is read into and what it is decoded into, and as many as it may
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
	 * @param chunks_in_flight The chunks it has open at once.
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
		whole
	};

	// One chunk: its file, while it is open, and its bytes, while they are
	// held.
	struct unit
	{
		std::string path;
		std::unique_ptr<boost::interprocess::file_mapping> file;
		std::vector<byte> encoded;
		bool missing = false;
	};

	void admit(file_transfer_scope &scope);
	void run(file_transfer_scope &scope, std::size_t unit, step_kind kind);

	void open(std::size_t unit) const;
	void read_whole(std::size_t unit) const;
	void decode(std::size_t unit) const;

	std::string m_directory;
	std::shared_ptr<const zarr_array_layout> m_layout;
	array m_destination;
	byte *m_array_data;
	image_chunk_regions m_chunks;
	bool m_asynchronous;
	std::size_t m_limit;
	std::size_t m_encoded_limit;

	// Written only by the task of each unit until the task reports, and
	// by the calls of the operation after it does.
	mutable std::vector<unit> m_units;

	std::size_t m_next;
	std::size_t m_active;
};

} // namespace zarr
} // namespace vitrio
