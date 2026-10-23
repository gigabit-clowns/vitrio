// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <formats/raw/raw_image_layout.hpp>

#include <system/file_handle.hpp>

#include <cstddef>
#include <string>

namespace vitrio
{

class array_ref;
class file_read_queue;
class image_transfer_plan;

/**
 * @brief Read regions of the values of a file through a queue of file reads.
 *
 * The regions are resolved, and so bounds checked, before any of them is
 * read. Where they lie in the file as they are to lie in the array, one
 * value after another in the data type and byte order it takes, they are
 * read straight into it: the regions that follow one another in the file as
 * one read, scattered over where they land.
 *
 * Otherwise the stretches of the file the regions reach are read into memory
 * of their own, and moved out of it. They are worked out and grouped into
 * steps as @ref image_read_schedule describes, on the grid of the elements of
 * the values, so that the values land aligned in that memory wherever they
 * lie in the file. A step is read while the one before it is moved, and a
 * stretch is moved as soon as it arrives.
 *
 * Whether it returns or throws, none of the reads it queues is outstanding
 * once it is done.
 *
 * @param path Path to the file, which what is thrown names.
 * @param file The file, open for reading.
 * @param file_size How many bytes the file holds.
 * @param layout Where and how the file holds its values.
 * @param destination The array the regions land in.
 * @param regions The regions to read and where each one lands.
 * @param queue The queue to read through, with no read outstanding.
 * @param byte_budget Most bytes a step reads into memory of its own, which
 * only a single stretch wider than it exceeds.
 * @throws std::invalid_argument If @p destination is not initialized, or if
 * a rank of the plan does not match the values or @p destination.
 * @throws std::out_of_range If a region is not contained in the values, or
 * does not fit in @p destination where it is placed.
 * @throws unsupported_operation_error If the data type of @p destination
 * can not be produced from the one of the values.
 * @throws image_file_error If the file can not be read where a region is.
 * @throws image_file_format_error If the file ends before a region does.
 */
void read_queued_regions(
	const std::string &path,
	file_handle_t file,
	std::size_t file_size,
	const raw_image_layout &layout,
	array_ref destination,
	const image_transfer_plan &regions,
	file_read_queue &queue,
	std::size_t byte_budget
);

} // namespace vitrio
