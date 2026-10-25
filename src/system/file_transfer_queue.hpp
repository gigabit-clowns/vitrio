// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_completion.hpp"
#include "file_transfer_submitter.hpp"

namespace vitrio
{

/**
 * @brief Transfers of files queued, then waited for by the thread that queued
 * them.
 *
 * A transfer is outstanding until @ref wait returns its completion.
 * Destroying a queue waits for the transfers that are outstanding.
 */
class file_transfer_queue
	: public file_transfer_submitter
{
public:
	file_transfer_queue() noexcept;
	~file_transfer_queue() override;

	/**
	 * @brief Wait until a transfer completes.
	 *
	 * Transfers that are queued are set on their way first.
	 *
	 * @return file_transfer_completion The completion of the transfer.
	 * @pre A transfer is outstanding.
	 * @throws std::system_error If the system can not be waited on. The
	 * transfers stay outstanding.
	 */
	virtual file_transfer_completion wait() = 0;
};

} // namespace vitrio
