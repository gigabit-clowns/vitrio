// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/export.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <memory>

namespace vitrio
{

class array_descriptor;
class completion_notifier;
class image_reader;

/**
 * @brief Where a scratch keeps its copies: a block of bytes, in main memory
 * or in a file, which it reads and writes through the operations of the
 * storage.
 *
 * The copies are values laid out in the block as an @ref array_descriptor
 * states: its extents and strides, in elements of its data type, and its
 * offset, in elements from the first byte of the block, which a value of
 * that type is aligned at. The storage serves regions of them into arrays
 * and takes regions of files into them, asynchronously, as transfers do:
 * either reports exactly once, a failure is reported rather than thrown, and
 * only a null notifier throws. Besides them the block holds a few bytes of
 * the scratch's own, such as which copies are whole, which are read and
 * written on the calling thread.
 *
 * @par Thread safety
 * Every method may be called concurrently, on different bytes of the block.
 * Bytes that are being written are not read meanwhile.
 */
class VITRIO_API image_scratch_storage
{
public:
	image_scratch_storage() noexcept;
	image_scratch_storage(const image_scratch_storage &other) = delete;
	image_scratch_storage(image_scratch_storage &&other) = delete;
	virtual ~image_scratch_storage();

	image_scratch_storage&
	operator=(const image_scratch_storage &other) = delete;
	image_scratch_storage& operator=(image_scratch_storage &&other) = delete;

	/**
	 * @brief Get the size of the block.
	 *
	 * @return std::size_t The number of bytes, which does not change.
	 */
	virtual std::size_t get_size() const noexcept = 0;

	/**
	 * @brief Read bytes of the block, before this returns.
	 *
	 * @param offset Where the bytes start, from the first of the block.
	 * @param bytes Where they go: as many as are read.
	 * @throws std::out_of_range If the bytes reach past the block.
	 * @throws image_file_error If the block can not be read.
	 */
	virtual void read(std::size_t offset, span<byte> bytes) const = 0;

	/**
	 * @brief Write bytes of the block, before this returns.
	 *
	 * @param offset Where the bytes start, from the first of the block.
	 * @param bytes What is written: as many as are written.
	 * @throws std::out_of_range If the bytes reach past the block.
	 * @throws image_file_error If the block can not be written.
	 */
	virtual void write(std::size_t offset, span<const byte> bytes) = 0;

	/**
	 * @brief Start reading regions of values the block holds into an array.
	 *
	 * The values are the file side of the plan, as a file is to
	 * @ref image_reader::read_async, and @p destination its array side: the
	 * same values land on the same elements, converted the same way, and
	 * the transfer keeps @p destination until it reports.
	 *
	 * @param values Where the values lie in the block. Within it.
	 * @param destination Where the regions land.
	 * @param regions The regions, at their position among the values, and
	 * where each one lands.
	 * @param done Where the outcome is reported. Must not be null.
	 * @throws std::invalid_argument If @p done is null.
	 */
	virtual void serve_async(
		const array_descriptor &values,
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) const = 0;

	/**
	 * @brief Start taking regions of a file into values the block holds.
	 *
	 * The file is the file side of the plan, as in
	 * @ref image_reader::read_async, and the values its array side.
	 *
	 * @param file A reader over the file, which is kept until the outcome is
	 * reported: the storage may read it later. Must not be null.
	 * @param values Where the values lie in the block. Within it.
	 * @param regions The regions, at their position in the file, and where
	 * each one lands among the values.
	 * @param done Where the outcome is reported, which is what reading the
	 * file reports, or what writing the block fails with, such as an
	 * std::invalid_argument for a null @p file. Must not be null.
	 * @throws std::invalid_argument If @p done is null.
	 */
	virtual void load_async(
		std::shared_ptr<const image_reader> file,
		const array_descriptor &values,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) = 0;
};

} // namespace vitrio
