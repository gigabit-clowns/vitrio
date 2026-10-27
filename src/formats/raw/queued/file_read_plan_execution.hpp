// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_plan.hpp"

#include <system/file_handle.hpp>
#include <system/file_transfer_completion.hpp>
#include <system/file_transfer_submitter.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace vitrio
{

/**
 * @brief The reads of a plan, carried out a step ahead as their stretches
 * arrive and are done with.
 *
 * A step is sent together with the one after it, and the step after that
 * once every read of the first has arrived and is done with, so that steps
 * two apart may read into the same memory. Each read is queued with its index
 * in the plan as its tag.
 */
class file_read_plan_execution
{
public:
	/**
	 * @brief Prepare to carry out the reads of a plan.
	 *
	 * @param path Path to the file, which what is thrown names. It must
	 * outlive the execution.
	 * @param file The file, open for reading.
	 * @param plan The reads, which must outlive the execution.
	 */
	file_read_plan_execution(
		const std::string &path,
		file_handle_t file,
		const file_read_plan &plan
	);

	file_read_plan_execution(const file_read_plan_execution &other) = delete;
	file_read_plan_execution(file_read_plan_execution &&other) = delete;
	~file_read_plan_execution();

	file_read_plan_execution&
	operator=(const file_read_plan_execution &other) = delete;
	file_read_plan_execution&
	operator=(file_read_plan_execution &&other) = delete;

	/**
	 * @brief Send the first steps.
	 *
	 * @param reads Where the reads are queued.
	 * @throws std::system_error If the system can not take a read.
	 */
	void start(file_transfer_submitter &reads);

	/**
	 * @brief Take the completion of a read of the plan.
	 *
	 * @param completion The completion, tagged with the index of the read.
	 * @return std::size_t The index of the read, whose stretch arrived whole.
	 * @throws image_file_error If the system failed the read.
	 * @throws image_file_format_error If the file ends before the stretch
	 * does.
	 */
	std::size_t take(const file_transfer_completion &completion) const;

	/**
	 * @brief Be done with a read that arrived, and send the step that frees,
	 * if any.
	 *
	 * @param read The index of the read.
	 * @param reads Where the reads are queued.
	 * @throws std::system_error If the system can not take a read.
	 */
	void release(std::size_t read, file_transfer_submitter &reads);

private:
	void send_ahead(file_transfer_submitter &reads);
	void send_step(std::size_t step, file_transfer_submitter &reads);

	const std::string &m_path;
	file_handle_t m_file;
	const file_read_plan &m_plan;

	// The reads of each step that were sent and are not done with.
	std::vector<std::size_t> m_pending;
	std::size_t m_next_read;
	std::size_t m_sent_steps;
	std::size_t m_done_steps;
};

} // namespace vitrio
