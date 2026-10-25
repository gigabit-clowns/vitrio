// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <file_transfer/file_transfer_operation.hpp>
#include <formats/raw/raw_image_layout.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <boost/interprocess/file_mapping.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace vitrio
{

/**
 * @brief Plan a read of regions of the values of a file, as an operation that
 * queues its reads.
 *
 * The regions are resolved, and so bounds checked, here. Where they lie in
 * the file as they are to lie in the array, one value after another in the
 * data type and byte order it takes, they are read straight into it, as
 * @ref direct_image_read_operation does.
 *
 * Otherwise the stretches of the file the regions reach are read into memory
 * of their own and moved out of it, as @ref staged_image_read_operation does.
 * They are worked out and grouped into steps as @ref image_transfer_schedule
 * describes, on the grid of the elements of the values, so that the values
 * land aligned in that memory wherever they lie in the file.
 *
 * @param path Path to the file, which what the operation throws names.
 * @param file The file, open for reading.
 * @param file_size How many bytes the file holds.
 * @param layout Where and how the file holds its values.
 * @param destination The array the regions land in.
 * @param regions The regions to read and where each one lands.
 * @param byte_budget Most bytes a step reads into memory of its own, which
 * only a single stretch wider than it exceeds.
 * @return std::unique_ptr<file_transfer_operation> The operation. Never
 * null.
 * @throws std::invalid_argument If @p destination is not initialized, or if
 * a rank of the plan does not match the values or @p destination.
 * @throws std::out_of_range If a region is not contained in the values, or
 * does not fit in @p destination where it is placed.
 */
std::unique_ptr<file_transfer_operation> make_image_read_operation(
	const std::string &path,
	std::shared_ptr<const boost::interprocess::file_mapping> file,
	std::size_t file_size,
	const raw_image_layout &layout,
	array destination,
	const image_transfer_plan &regions,
	std::size_t byte_budget
);

} // namespace vitrio
