// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_transfer_plan.hpp>

namespace vitrio
{

class array_ref;

class image_reader;

/**
 * @brief What a scratch holds of one image file.
 *
 * A copy of some of what the file holds, kept where it is faster to reach
 * than the file. Which parts of the file an entry keeps, and how it keeps
 * them, is implementation's own business. It is asked in regions of the file:
 * a read is answered with the regions it did not serve, and it takes in what it
 * has room for of the regions it is offered.
 *
 * Nothing an entry has taken in is dropped, so a region it once served it
 * serves from then on.
 *
 * @par Thread safety
 * Every method may be called concurrently.
 *
 * @see image_scratch
 */
class VITRIO_API image_scratch_entry
{
public:
	image_scratch_entry() noexcept;
	image_scratch_entry(const image_scratch_entry &other) = delete;
	image_scratch_entry(image_scratch_entry &&other) = delete;
	virtual ~image_scratch_entry();

	image_scratch_entry& operator=(const image_scratch_entry &other) = delete;
	image_scratch_entry& operator=(image_scratch_entry &&other) = delete;

	/**
	 * @brief Read the regions of a plan that are held, and answer the rest.
	 *
	 * The file is the file side of the plan and @p destination its array
	 * side, as in @ref image_reader::read, and a region that is held is read
	 * as that would read it: the same values land on the same elements,
	 * converted the same way.
	 *
	 * A region is held when everything it spans is. One that is held only
	 * in part is not read at all, and neither is a plan that does not have
	 * the rank of the file.
	 *
	 * @param destination Where the regions that are held are written. Must
	 * be initialized.
	 * @param regions The regions to read and where each one lands.
	 * @return image_transfer_plan The regions that were not read, of the
	 * shape of @p regions and in the order they have in it. Empty when every
	 * region was read.
	 * @throws std::invalid_argument If @p destination is not initialized.
	 * @throws std::out_of_range If a region that is held does not fit in
	 * @p destination where it is placed.
	 * @throws unsupported_operation_error If the data type of @p destination
	 * can not be produced from the one of the file.
	 */
	virtual image_transfer_plan read(
		array_ref destination,
		const image_transfer_plan &regions
	) const = 0;

	/**
	 * @brief Take in regions of the file, out of a reader over it.
	 *
	 * Takes in what it has room for of what the regions span, and may take
	 * in more of the file than they span. A region none of which it has room
	 * for is passed over, and so is a plan that does not have the rank of
	 * the file. What is already held is not read again.
	 *
	 * Only the file side of the plan plays a part.
	 *
	 * If reading @p file fails, what was being taken in is not held.
	 *
	 * @param file A reader over the file this entry is of.
	 * @param regions The regions to take in.
	 * @throws image_format_error If the file turns out to be malformed or
	 * truncated where it is read.
	 */
	virtual void store(
		const image_reader &file,
		const image_transfer_plan &regions
	) = 0;
};

} // namespace vitrio
