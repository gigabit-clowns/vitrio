// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_transfer_port.hpp"

#include <memory>

namespace vitrio
{

/**
 * @brief Opens the ports of file transfers that threads wait on.
 *
 * @par Thread safety
 * @ref open may be called concurrently.
 */
class file_transfer_port_opener
{
public:
	file_transfer_port_opener() noexcept;
	file_transfer_port_opener(const file_transfer_port_opener &other) = delete;
	file_transfer_port_opener(file_transfer_port_opener &&other) = delete;
	virtual ~file_transfer_port_opener();

	file_transfer_port_opener&
	operator=(const file_transfer_port_opener &other) = delete;
	file_transfer_port_opener&
	operator=(file_transfer_port_opener &&other) = delete;

	/**
	 * @brief Open a port.
	 *
	 * @return std::unique_ptr<file_transfer_port> The port, with no
	 * transfer outstanding. Never null.
	 */
	virtual std::unique_ptr<file_transfer_port> open() const = 0;
};

} // namespace vitrio
