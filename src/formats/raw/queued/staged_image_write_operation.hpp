// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_write_plan.hpp"
#include "writable_image_file.hpp"

#include <file_transfer/file_transfer_operation.hpp>
#include <formats/raw/planning/image_region_runs.hpp>
#include <formats/raw/planning/image_transfer_schedule.hpp>
#include <formats/raw/raw_image_layout.hpp>
#include <strided_transfer/image_region_write_walk.hpp>

#include <vitrio/array/const_array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>

#include <memory/byte_order.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace vitrio
{

/**
 * @brief Writes regions of the values of a file through memory of its own,
 * where their values are converted on their way from the array.
 *
 * A step of a schedule is staged in memory of its own: each of its stretches
 * is converted into it by a task on the executor of computations, then
 * written as the runs its regions cover, and nothing between them. A step is
 * converted while the one before it is written, so the steps take turns at
 * two halves of that memory. The operation keeps the file open and the
 * array alive until it is done.
 */
class staged_image_write_operation final
	: public file_transfer_operation
{
public:
	/**
	 * @brief Plan the writes of the stretches a schedule groups into steps.
	 *
	 * @param file The file, open for writing.
	 * @param layout Where and how the file holds its values.
	 * @param source The array the regions come from.
	 * @param walk The regions, resolved against the file and @p source.
	 * @param schedule The stretches of the file the regions span, grouped
	 * into steps, merged only where they touch.
	 * @param runs The runs each region covers.
	 */
	staged_image_write_operation(
		std::shared_ptr<writable_image_file> file,
		const raw_image_layout &layout,
		const_array source,
		image_region_write_walk walk,
		const image_transfer_schedule &schedule,
		const image_region_runs &runs
	);

	~staged_image_write_operation() override;

	void start(file_transfer_scope &scope) override;

	void on_transfer(
		const file_transfer_completion &completion,
		file_transfer_scope &scope
	) override;

	void on_task(
		std::uint64_t tag,
		std::exception_ptr error,
		file_transfer_scope &scope
	) override;

private:
	class conversion;

	// Moves the regions of a stretch from the array into its staging.
	void convert(std::size_t range) const;

	// Converts the steps that have staging free, past the steps that are
	// done.
	void send_ahead(file_transfer_scope &scope);

	std::shared_ptr<writable_image_file> m_file;

	const_array m_source;
	const byte *m_array_data;
	numerical_type m_array_type;

	numerical_type m_file_type;
	byte_order m_file_order;
	std::size_t m_data_offset;

	image_region_write_walk m_walk;

	// Where each stretch starts in the file, the first of its regions and
	// how many it has, where it is staged, and the step it falls in.
	std::vector<std::size_t> m_range_offsets;
	std::vector<std::size_t> m_first_regions;
	std::vector<std::size_t> m_region_counts;
	std::vector<byte*> m_staged;
	std::vector<std::size_t> m_range_steps;

	// The first stretch of each step, and one past the last.
	std::vector<std::size_t> m_step_first_ranges;

	std::unique_ptr<byte[]> m_staging;

	// One group a stretch, so a group and its stretch share their index.
	file_write_plan m_plan;

	// The conversions and writes of each step that are under way.
	std::vector<std::size_t> m_pending;
	std::size_t m_sent_steps;
	std::size_t m_done_steps;
};

} // namespace vitrio
