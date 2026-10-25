// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_write_plan.hpp"
#include "writable_image_file.hpp"

#include <file_transfer/file_transfer_operation.hpp>
#include <formats/raw/raw_image_layout.hpp>
#include <strided_transfer/image_region_write_walk.hpp>

#include <vitrio/array/const_array.hpp>

#include <memory>

namespace vitrio
{

/**
 * @brief Writes regions of the values of a file straight from an array, where
 * they lie in the array as they are to lie in the file: one value after
 * another on both sides, in the data type and byte order the file takes.
 *
 * The regions that follow one another in the file are one write, gathered
 * from where they lie, and every write is sent at once. The operation keeps
 * the file open and the array alive until it is done.
 */
class direct_image_write_operation final
	: public file_transfer_operation
{
public:
	/**
	 * @brief Plan the writes of regions.
	 *
	 * @param file The file, open for writing.
	 * @param layout Where and how the file holds its values.
	 * @param source The array the regions come from.
	 * @param walk The regions, resolved against the file and @p source.
	 * @pre The regions lie in @p source as they are to lie in the file.
	 */
	direct_image_write_operation(
		std::shared_ptr<writable_image_file> file,
		const raw_image_layout &layout,
		const_array source,
		const image_region_write_walk &walk
	);

	~direct_image_write_operation() override;

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
	std::shared_ptr<writable_image_file> m_file;
	const_array m_source;
	file_write_plan m_plan;
};

} // namespace vitrio
