// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_handle.hpp"

namespace vitrio
{

/**
 * @brief A counter the kernel keeps behind a file, an eventfd of Linux, which
 * one thread signals and another reads.
 *
 * A read of eight bytes of the file waits until the counter is not zero,
 * then returns its value and resets it, so a thread that waits on reads of
 * files, as through an io_uring, can wait on it too.
 */
class event_counter
{
public:
	/**
	 * @brief Open a counter that starts at zero.
	 *
	 * Its file blocks a read while the counter is zero, rather than failing
	 * it, which is what an io_uring waits on.
	 *
	 * @throws std::system_error If the kernel can not open it.
	 */
	event_counter();
	event_counter(const event_counter &other) = delete;
	event_counter(event_counter &&other) = delete;
	~event_counter();

	event_counter& operator=(const event_counter &other) = delete;
	event_counter& operator=(event_counter &&other) = delete;

	/**
	 * @brief Get the file the counter is read through.
	 *
	 * @return file_handle_t The file.
	 */
	file_handle_t get_file() const noexcept;

	/**
	 * @brief Add one to the counter, which ends a read waiting on it.
	 *
	 * @par Thread safety
	 * May be called from any thread.
	 */
	void signal() noexcept;

private:
	file_handle_t m_file;
};

} // namespace vitrio
