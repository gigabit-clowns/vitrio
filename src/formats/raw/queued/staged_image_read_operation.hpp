// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_plan.hpp"
#include "file_read_plan_execution.hpp"

#include <file_transfer/file_transfer_operation.hpp>
#include <formats/raw/planning/image_read_schedule.hpp>
#include <formats/raw/raw_image_layout.hpp>
#include <strided_transfer/image_region_read_walk.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>

#include <memory/byte_order.hpp>

#include <boost/interprocess/file_mapping.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace vitrio
{

/**
 * @brief Reads regions of the values of a file into memory of its own, a
 * step at a time, and moves them into an array, converting them, on the
 * executor of computations.
 *
 * The stretches of the file a schedule groups into steps are read into two
 * halves of the memory, a step into each in turn. A stretch is moved out as
 * soon as it arrives, by a task of its own, and the step after next is read
 * into its half once every stretch of the step there is moved. The operation
 * keeps the file open and the array alive until it is done.
 */
class staged_image_read_operation final
	: public file_transfer_operation
{
public:
	/**
	 * @brief Plan the reads of the stretches a schedule groups into steps.
	 *
	 * @param path Path to the file, which what is thrown names.
	 * @param file The file, open for reading.
	 * @param layout Where and how the file holds its values.
	 * @param destination The array the regions land in.
	 * @param walk The regions, resolved against the file and @p destination.
	 * @param schedule The stretches of the file the regions reach, grouped
	 * into steps, on the grid of the elements of the values.
	 */
	staged_image_read_operation(
		std::string path,
		std::shared_ptr<const boost::interprocess::file_mapping> file,
		const raw_image_layout &layout,
		array destination,
		image_region_read_walk walk,
		const image_read_schedule &schedule
	);
	~staged_image_read_operation() override;

	void start(file_transfer_scope &scope) override;

	void on_read(
		const file_read_completion &completion,
		file_transfer_scope &scope
	) override;

	void on_task(
		std::uint64_t tag,
		std::exception_ptr error,
		file_transfer_scope &scope
	) override;

private:
	class conversion;

	// Moves the regions of a stretch that arrived into the array.
	void convert(std::size_t range) const;

	std::string m_path;
	std::shared_ptr<const boost::interprocess::file_mapping> m_file;

	array m_destination;
	byte *m_array_data;
	numerical_type m_array_type;

	numerical_type m_file_type;
	byte_order m_file_order;
	std::size_t m_data_offset;

	image_region_read_walk m_walk;

	// The first of the regions of each stretch, and how many it has.
	std::vector<std::size_t> m_first_regions;
	std::vector<std::size_t> m_region_counts;

	std::vector<byte> m_staging;

	// One read a stretch, so a read and its stretch share their index.
	file_read_plan m_plan;
	file_read_plan_execution m_execution;
};

} // namespace vitrio
