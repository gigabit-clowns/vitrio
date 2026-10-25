// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_read_plan.hpp"
#include "file_read_plan_execution.hpp"

#include <file_transfer/file_transfer_operation.hpp>
#include <formats/raw/raw_image_layout.hpp>
#include <strided_transfer/image_region_read_walk.hpp>

#include <vitrio/array/array.hpp>

#include <boost/interprocess/file_mapping.hpp>

#include <memory>
#include <string>

namespace vitrio
{

/**
 * @brief Reads regions of the values of a file straight into an array, where
 * they lie in the file as they are to lie in the array: one value after
 * another on both sides, in the data type and byte order the array takes.
 *
 * The regions that follow one another in the file are one read, scattered
 * over where they land, and every read is sent at once. The operation keeps
 * the file open and the array alive until it is done.
 */
class direct_image_read_operation final
	: public file_transfer_operation
{
public:
	/**
	 * @brief Plan the reads of regions.
	 *
	 * @param path Path to the file, which what is thrown names.
	 * @param file The file, open for reading.
	 * @param layout Where and how the file holds its values.
	 * @param destination The array the regions land in.
	 * @param walk The regions, resolved against the file and @p destination.
	 * @pre The regions lie in the file as they are to lie in @p destination.
	 */
	direct_image_read_operation(
		std::string path,
		std::shared_ptr<const boost::interprocess::file_mapping> file,
		const raw_image_layout &layout,
		array destination,
		const image_region_read_walk &walk
	);
	~direct_image_read_operation() override;

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
	std::string m_path;
	std::shared_ptr<const boost::interprocess::file_mapping> m_file;
	array m_destination;
	file_read_plan m_plan;
	file_read_plan_execution m_execution;
};

} // namespace vitrio
