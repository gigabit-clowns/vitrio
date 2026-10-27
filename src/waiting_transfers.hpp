// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_transfer_plan.hpp>

namespace vitrio
{

class array;
class const_array;
class image_reader;
class image_writer;

/**
 * @brief Read through a reader and wait for the read.
 *
 * For code whose own contract is to be done before it returns.
 *
 * @param reader The reader.
 * @param destination Where the regions are written.
 * @param regions The regions to read and where each one lands.
 * @throws Whatever the read reports, as
 * @ref image_reader::read_async lists.
 */
void read_and_wait(
	const image_reader &reader,
	array destination,
	image_transfer_plan regions
);

/**
 * @brief Write through a writer and wait for the write.
 *
 * For code whose own contract is to be done before it returns.
 *
 * @param writer The writer.
 * @param source The values to write.
 * @param regions The regions to write and where each one comes from.
 * @throws Whatever the write reports, as
 * @ref image_writer::write_async lists.
 */
void write_and_wait(
	image_writer &writer,
	const_array source,
	image_transfer_plan regions
);

/**
 * @brief Flush a writer and wait for the flush.
 *
 * For code whose own contract is to be done before it returns.
 *
 * @param writer The writer.
 * @throws Whatever the flush reports, as
 * @ref image_writer::flush_async lists.
 */
void flush_and_wait(image_writer &writer);

} // namespace vitrio
