// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "writable_image_file.hpp"

#include <file_transfer/file_transfer_operation.hpp>
#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/array/const_array.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <cstddef>
#include <memory>

namespace vitrio
{

/**
 * @brief Plan a write of regions of the values of a file, as an operation
 * that queues its writes.
 *
 * The regions are resolved, and so bounds checked, here. Where they lie in
 * the array as they are to lie in the file, one value after another in the
 * data type and byte order it takes, they are written straight from it, as
 * @ref direct_image_write_operation does.
 *
 * Otherwise they are converted into memory of their own and written from
 * there, as @ref staged_image_write_operation does. The stretches they span
 * are worked out and grouped into steps as @ref image_transfer_schedule
 * describes, merged only where they touch, since what lies between them in
 * the file is not to be written.
 *
 * @param file The file, open for writing.
 * @param layout Where and how the file holds its values.
 * @param source The array the regions come from.
 * @param regions The regions to write and where each one comes from.
 * @param byte_budget Most bytes a step stages in memory of its own, which
 * only a single stretch wider than it exceeds.
 * @return std::unique_ptr<file_transfer_operation> The operation. Never
 * null.
 * @throws std::invalid_argument If @p source is not initialized, or if a
 * rank of the plan does not match the values or @p source.
 * @throws std::out_of_range If a region is not contained in the values, or
 * is not contained in @p source where it is taken from.
 */
std::unique_ptr<file_transfer_operation> make_image_write_operation(
	std::shared_ptr<writable_image_file> file,
	const raw_image_layout &layout,
	const_array source,
	const image_transfer_plan &regions,
	std::size_t byte_budget
);

} // namespace vitrio
