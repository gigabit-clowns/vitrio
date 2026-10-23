// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_port_opener.hpp"

#include <cstddef>

namespace vitrio
{

/**
 * @brief Opens ports whose reads the system carries out while their thread
 * goes on.
 *
 * A port is an io_uring, which only builds with liburing have. Where the
 * system refuses the process one more, such as when it is short of the
 * memory it lets the process lock, the port reads as it is asked instead,
 * and the refusal is logged.
 */
class asynchronous_file_read_port_opener final
	: public file_read_port_opener
{
public:
	/**
	 * @brief Construct an opener of ports with room for a number of reads.
	 *
	 * @param capacity How many reads the kernel may carry out at once for a
	 * port. Not zero.
	 * @pre The process can have asynchronous ports: @ref is_supported.
	 */
	explicit asynchronous_file_read_port_opener(std::size_t capacity) noexcept;
	~asynchronous_file_read_port_opener() override;

	/**
	 * @brief Tell whether the process can have asynchronous ports of file
	 * reads.
	 *
	 * @return bool Whether it can. The same for as long as the process
	 * lives.
	 */
	static bool is_supported() noexcept;

	std::unique_ptr<file_read_port> open() const override;

private:
	std::size_t m_capacity;
};

} // namespace vitrio
