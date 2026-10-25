// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <memory>

namespace vitrio
{

class array;
class completion_notifier;
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
	 * @brief Start reading the regions of a plan that are held, and answer
	 * the rest.
	 *
	 * The file is the file side of the plan and @p destination its array
	 * side, as in @ref image_reader::read_async, and a region that is held
	 * is read as that would read it: the same values land on the same
	 * elements, converted the same way.
	 *
	 * A region is held when everything it spans is. One that is held only
	 * in part is not read at all, and neither is a plan that does not have
	 * the rank of the file.
	 *
	 * The read of what is held may still be under way when this returns. It
	 * reports to @p done exactly once, as a read of a file does: when every
	 * region that is held has landed, at once if none is, or with what made
	 * it fail. It keeps @p destination until then.
	 *
	 * @param destination Where the regions that are held are written.
	 * @param regions The regions to read and where each one lands.
	 * @param done Where the outcome is reported. Must not be null.
	 * @return image_transfer_plan The regions that are not read, of the
	 * shape of @p regions and in the order they have in it. Empty when every
	 * region is, and all of them when the read fails at once.
	 * @throws std::invalid_argument If @p done is null. Every other failure
	 * is reported to @p done, as that of a read of a file is.
	 */
	virtual image_transfer_plan read_async(
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) const = 0;

	/**
	 * @brief Take in regions of the file, out of a reader over it, and
	 * report once they are held.
	 *
	 * Takes in what it has room for of what the regions span, and may take
	 * in more of the file than they span. A region none of which it has room
	 * for is passed over, and so is a plan that does not have the rank of
	 * the file. What is already held is not read again, and what is already
	 * being taken in, by another call, is waited for rather than read again.
	 *
	 * Only the file side of the plan plays a part.
	 *
	 * Reports to @p done exactly once, when everything the regions need is
	 * held or failed to be. What failed to be taken in is not held, and the
	 * failure is reported, such as an image_file_format_error for a file
	 * that turns out to be malformed or truncated where it is read. Only a
	 * null notifier throws.
	 *
	 * @param file A reader over the file this entry is of, which is kept
	 * until the outcome is reported. Must not be null.
	 * @param regions The regions to take in.
	 * @param done Where the outcome is reported. Must not be null.
	 * @throws std::invalid_argument If @p done is null.
	 */
	virtual void store_async(
		std::shared_ptr<const image_reader> file,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) = 0;
};

} // namespace vitrio
