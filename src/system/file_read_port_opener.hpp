// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_port.hpp"

#include <memory>

namespace vitrio
{

/**
 * @brief Opens the ports of file reads that threads wait on.
 *
 * @par Thread safety
 * @ref open may be called concurrently.
 */
class file_read_port_opener
{
public:
	file_read_port_opener() noexcept;
	file_read_port_opener(const file_read_port_opener &other) = delete;
	file_read_port_opener(file_read_port_opener &&other) = delete;
	virtual ~file_read_port_opener();

	file_read_port_opener&
	operator=(const file_read_port_opener &other) = delete;
	file_read_port_opener& operator=(file_read_port_opener &&other) = delete;

	/**
	 * @brief Open a port.
	 *
	 * @return std::unique_ptr<file_read_port> The port, with no read
	 * outstanding. Never null.
	 */
	virtual std::unique_ptr<file_read_port> open() const = 0;
};

} // namespace vitrio
