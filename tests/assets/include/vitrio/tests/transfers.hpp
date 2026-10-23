// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array.hpp>
#include <vitrio/array/const_array.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_writer.hpp>

#include <memory>

namespace vitrio
{
namespace test
{

/**
 * @brief Read through a reader and wait for the read.
 *
 * @param reader The reader.
 * @param destination Where the regions are written. The read shares it.
 * @param regions The regions to read and where each one lands.
 * @throws Whatever the read reports.
 */
inline void read_now(
	const image_reader &reader,
	array &destination,
	const image_transfer_plan &regions
)
{
	const auto done = std::make_shared<counting_completion>(1);
	reader.read_async(destination.share(), regions, done);
	done->get();
}

/**
 * @brief Write through a writer and wait for the write.
 *
 * @param writer The writer.
 * @param source The values to write. The write shares them.
 * @param regions The regions to write and where each one comes from.
 * @throws Whatever the write reports.
 */
inline void write_now(
	image_writer &writer,
	const array &source,
	const image_transfer_plan &regions
)
{
	const auto done = std::make_shared<counting_completion>(1);
	writer.write_async(source.share_const(), regions, done);
	done->get();
}

/**
 * @brief Write through a writer and wait for the write.
 *
 * @param writer The writer.
 * @param source The values to write. The write shares them.
 * @param regions The regions to write and where each one comes from.
 * @throws Whatever the write reports.
 */
inline void write_now(
	image_writer &writer,
	const const_array &source,
	const image_transfer_plan &regions
)
{
	const auto done = std::make_shared<counting_completion>(1);
	writer.write_async(source.share(), regions, done);
	done->get();
}

/**
 * @brief Flush a writer and wait for the flush.
 *
 * @param writer The writer.
 * @throws Whatever the flush reports.
 */
inline void flush_now(image_writer &writer)
{
	const auto done = std::make_shared<counting_completion>(1);
	writer.flush_async(done);
	done->get();
}

} // namespace test
} // namespace vitrio
