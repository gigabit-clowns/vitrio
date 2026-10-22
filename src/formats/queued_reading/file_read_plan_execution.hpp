// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_plan.hpp"

#include <system/file_handle.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace vitrio
{

class file_read_queue;

/**
 * @brief The reads of a plan, carried out through a queue a step ahead.
 *
 * The reads are taken one at a time, as their stretches arrive. A step is
 * sent together with the one after it, and the step after that once every
 * read of the first has been taken and the last of them is done with, so
 * that steps two apart may read into the same memory.
 *
 * Destroying an execution waits for the reads that are still outstanding,
 * so that none of them outlives the memory it reads into.
 */
class file_read_plan_execution
{
public:
	/**
	 * @brief Prepare to carry out the reads of a plan.
	 *
	 * @param path Path to the file, which what is thrown names.
	 * @param file The file, open for reading.
	 * @param plan The reads, which must outlive the execution.
	 * @param queue The queue to read through, with no read outstanding,
	 * which must outlive the execution.
	 */
	file_read_plan_execution(
		const std::string &path,
		file_handle_t file,
		const file_read_plan &plan,
		file_read_queue &queue
	);

	file_read_plan_execution(const file_read_plan_execution &other) = delete;
	file_read_plan_execution(file_read_plan_execution &&other) = delete;
	~file_read_plan_execution();

	file_read_plan_execution&
	operator=(const file_read_plan_execution &other) = delete;
	file_read_plan_execution&
	operator=(file_read_plan_execution &&other) = delete;

	/**
	 * @brief Wait for the next read whose stretch arrives whole.
	 *
	 * The read taken before is done with once this is called.
	 *
	 * @param read Set to the index of the read in the plan.
	 * @return bool Whether a read was taken. False once every one has been.
	 * @throws std::system_error If the system fails a read, or can not take
	 * one.
	 * @throws image_file_format_error If the file ends before a stretch does.
	 */
	bool take_next(std::size_t &read);

private:
	void release_taken() noexcept;

	void send_ahead();

	void send_step(std::size_t step);

	void wait_out() noexcept;

	const std::string &m_path;
	file_handle_t m_file;
	const file_read_plan &m_plan;
	file_read_queue &m_queue;

	// The reads of each step that were sent and are not done with.
	std::vector<std::size_t> m_pending;
	std::size_t m_outstanding;

	std::size_t m_next_read;
	std::size_t m_sent_steps;
	std::size_t m_done_steps;

	bool m_taken;
	std::size_t m_taken_read;
};

} // namespace vitrio
