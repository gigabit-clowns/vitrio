// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_completion.hpp"
#include "file_read_submitter.hpp"

namespace vitrio
{

/**
 * @brief Reads of files queued, then waited for by the thread that queued
 * them.
 *
 * A read is outstanding until @ref wait returns its completion. Outstanding
 * reads complete in any order. Destroying a queue waits for the reads that
 * are outstanding.
 */
class file_read_queue
	: public file_read_submitter
{
public:
	file_read_queue() noexcept;
	~file_read_queue() override;

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
