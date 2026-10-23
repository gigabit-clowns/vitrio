// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_handle.hpp"

#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <cstdint>

namespace vitrio
{

/**
 * @brief Where reads of stretches of files, each scattered over buffers, are
 * queued.
 *
 * A read is outstanding from when it is queued until its completion is handed
 * back, as the implementation says where, and the memory of its buffers must
 * stay valid as long. A read fills its buffers in order, from the first, and
 * stops short of filling them only where the file ends. Its completion carries
 * the tag it was queued with.
 */
class file_read_submitter
{
public:
	file_read_submitter() noexcept;
	file_read_submitter(const file_read_submitter &other) = delete;
	file_read_submitter(file_read_submitter &&other) = delete;
	virtual ~file_read_submitter();

	file_read_submitter& operator=(const file_read_submitter &other) = delete;
	file_read_submitter& operator=(file_read_submitter &&other) = delete;

	/**
	 * @brief Queue a read of a stretch of a file.
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
	virtual void queue(
		file_handle_t file,
		std::uint64_t offset,
		span<const memory_range> buffers,
		std::uint64_t tag
	) = 0;
};

} // namespace vitrio
