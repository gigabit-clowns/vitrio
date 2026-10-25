// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_completion.hpp"
#include "file_transfer_submitter.hpp"

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
class file_transfer_queue
	: public file_transfer_submitter
{
public:
	file_transfer_queue() noexcept;
	~file_transfer_queue() override;

	/**
	 * @brief Wait until a read completes.
	 *
	 * Reads that are queued are set on their way first.
	 *
	 * @return file_transfer_completion The completion of the read.
	 * @pre A read is outstanding.
	 * @throws std::system_error If the system can not be waited on. The
	 * reads stay outstanding.
	 */
	virtual file_transfer_completion wait() = 0;
};

} // namespace vitrio
