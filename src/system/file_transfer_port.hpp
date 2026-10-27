// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_completion.hpp"
#include "file_transfer_submitter.hpp"

#include <boost/optional.hpp>

namespace vitrio
{

/**
 * @brief Transfers of files queued and waited for by one thread, whose wait
 * any thread can end.
 *
 * It is what a thread waits on when it waits both for its transfers and for
 * work other threads hand it: they hand it the work, then @ref wake the port.
 *
 * A transfer is outstanding until @ref wait returns its completion.
 * Destroying a port waits for the transfers that are outstanding.
 *
 * @par Thread safety
 * @ref wake may be called from any thread, at any time while the port lives.
 * Everything else is called from one thread at a time.
 */
class file_transfer_port
	: public file_transfer_submitter
{
public:
	file_transfer_port() noexcept;
	~file_transfer_port() override;

	/**
	 * @brief Keep the waits of this port from blocking until one of them
	 * returns no completion.
	 *
	 * A wait under way returns, and one that starts later returns at once.
	 * One wait returning no completion may answer several wakes, and several
	 * may answer wakes that came close together.
	 */
	virtual void wake() noexcept = 0;

	/**
	 * @brief Wait until a transfer completes or the port is woken.
	 *
	 * Transfers that are queued are set on their way first. A port with no
	 * transfer outstanding waits until it is woken.
	 *
	 * @return boost::optional<file_transfer_completion> The completion of the
	 * transfer, or none if the port was woken.
	 * @throws std::system_error If the system can not be waited on. The
	 * transfers stay outstanding.
	 */
	virtual boost::optional<file_transfer_completion> wait() = 0;
};

} // namespace vitrio
