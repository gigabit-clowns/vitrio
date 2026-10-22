// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_handle.hpp"
#include "file_read_completion.hpp"

#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <cstdint>

namespace vitrio
{

/**
 * @brief Reads of stretches of files, each scattered over buffers, queued and
 * then waited for.
 *
 * A read is outstanding from when it is queued until @ref wait returns its
 * completion, and the memory of its buffers must stay valid as long.
 * Outstanding reads complete in any order. A read fills its buffers in order,
 * from the first, and stops short of filling them only where the file ends.
 * Destroying a queue waits for the reads that are outstanding.
 */
class file_read_queue
{
public:
	file_read_queue() noexcept;
	file_read_queue(const file_read_queue &other) = delete;
	file_read_queue(file_read_queue &&other) = delete;
	virtual ~file_read_queue();

	file_read_queue& operator=(const file_read_queue &other) = delete;
	file_read_queue& operator=(file_read_queue &&other) = delete;

	/**
	 * @brief Queue a read of a stretch of a file.
	 *
	 * @param file The file to read.
	 * @param offset Where in the file the stretch starts, in bytes.
	 * @param buffers Where its bytes go, in order: as many as the stretch
	 * covers.
	 * @param tag What the completion of the read carries, to tell it from
	 * the others.
	 * @throws std::system_error If the system can not take the read.
	 */
	virtual void queue(
		file_handle_t file,
		std::uint64_t offset,
		span<const memory_range> buffers,
		std::uint64_t tag
	) = 0;

	/**
	 * @brief Wait until a read completes.
	 *
	 * Reads that are queued are set on their way first.
	 *
	 * @return file_read_completion The completion of the read.
	 * @pre A read is outstanding.
	 * @throws std::system_error If the system can not be waited on. The
	 * reads stay outstanding.
	 */
	virtual file_read_completion wait() = 0;
};

} // namespace vitrio
