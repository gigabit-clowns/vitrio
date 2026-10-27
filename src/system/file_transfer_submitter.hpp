// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_handle.hpp"

#include <vitrio/const_memory_range.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <cstdint>

namespace vitrio
{

/**
 * @brief Where transfers of files are queued: reads and writes of stretches
 * of files, each scattered over buffers, and syncs of what was written.
 *
 * A transfer is outstanding from when it is queued until its completion is
 * handed back, as the implementation says where, and the memory of its
 * buffers must stay valid as long. Its completion carries the tag it was
 * queued with. Outstanding transfers complete in any order: one that must
 * follow another is queued once the other has completed.
 */
class file_transfer_submitter
{
public:
	file_transfer_submitter() noexcept;
	file_transfer_submitter(const file_transfer_submitter &other) = delete;
	file_transfer_submitter(file_transfer_submitter &&other) = delete;
	virtual ~file_transfer_submitter();

	file_transfer_submitter&
	operator=(const file_transfer_submitter &other) = delete;
	file_transfer_submitter&
	operator=(file_transfer_submitter &&other) = delete;

	/**
	 * @brief Queue a read of a stretch of a file.
	 *
	 * The read fills its buffers in order, from the first, and stops short
	 * of filling them only where the file ends. Its completion carries how
	 * many bytes it read.
	 *
	 * @param file The file to read.
	 * @param offset Where in the file the stretch starts, in bytes.
	 * @param buffers Where its bytes go, in order: as many as the stretch
	 * covers.
	 * @param tag What the completion of the read carries, to tell it from
	 * the others.
	 * @throws std::system_error If the system can not take the read. It is
	 * then not queued.
	 */
	virtual void queue_read(
		file_handle_t file,
		std::uint64_t offset,
		span<const memory_range> buffers,
		std::uint64_t tag
	) = 0;

	/**
	 * @brief Queue a write of a stretch of a file.
	 *
	 * The write takes the bytes of its buffers in order, from the first. Its
	 * completion carries how many bytes it wrote, which is every byte of the
	 * buffers unless it failed.
	 *
	 * @param file The file to write, open for writing.
	 * @param offset Where in the file the stretch starts, in bytes.
	 * @param buffers Where its bytes come from, in order: as many as the
	 * stretch covers.
	 * @param tag What the completion of the write carries, to tell it from
	 * the others.
	 * @throws std::system_error If the system can not take the write. It is
	 * then not queued.
	 */
	virtual void queue_write(
		file_handle_t file,
		std::uint64_t offset,
		span<const const_memory_range> buffers,
		std::uint64_t tag
	) = 0;

	/**
	 * @brief Queue a sync of a file: making what was written to it reach the
	 * storage.
	 *
	 * It covers the writes whose completions were handed back before it was
	 * queued, whoever queued them. Its completion carries no byte.
	 *
	 * @param file The file to sync, open for writing.
	 * @param tag What the completion of the sync carries, to tell it from
	 * the others.
	 * @throws std::system_error If the system can not take the sync. It is
	 * then not queued.
	 */
	virtual void queue_sync(file_handle_t file, std::uint64_t tag) = 0;
};

} // namespace vitrio
