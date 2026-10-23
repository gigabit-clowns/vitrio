// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <memory>

namespace vitrio
{

class array;
class completion_notifier;
class image_descriptor;
class image_metadata;

/**
 * @brief Abstract read-only view of one image file.
 *
 * A reader is opened over one image file and exposes its contents as one
 * rectangular ND array. Every read is a set of hyperrectangles of that
 * array, which is the only access pattern there is: image @c i of an
 * @c (N,H,W) stack is the region of extents @c (H,W) at offset @c (i,0,0),
 * a patch of an @c (H,W) micrograph is the region of extents @c (h,w) at
 * offset @c (y,x), and a whole volume is the region covering everything.
 * Reading in a random or in a sequential order is a property of a sequence
 * of calls rather than of one, so it is nothing a reader distinguishes.
 *
 * A read takes every region at once rather than one at a time. That is the
 * only shape in which a reader can see enough to order and merge the
 * accesses it is about to make, and it keeps the per region cost to
 * arithmetic.
 *
 * Everything a reader reports is fixed when it is opened, and reading does
 * not change it.
 */
class VITRIO_API image_reader
{
public:
	image_reader() noexcept;
	image_reader(const image_reader &other) = delete;
	image_reader(image_reader &&other) = delete;
	virtual ~image_reader();

	image_reader& operator=(const image_reader &other) = delete;
	image_reader& operator=(image_reader &&other) = delete;

	/**
	 * @brief Get the shape and data type of what the file holds.
	 *
	 * The extents are those of the file in the order it stores its axes,
	 * slowest first. How it lays its elements out within them is the
	 * format's own business and is not reported. The data type is the one a
	 * read produces without converting, which is therefore the cheapest type
	 * to ask a destination for.
	 *
	 * A file whose format can not tell a stack of one image or volume from a
	 * single one is described as the single one.
	 *
	 * @return const image_descriptor& The descriptor. It refers to storage
	 * owned by this reader.
	 */
	virtual const image_descriptor& get_descriptor() const noexcept = 0;

	/**
	 * @brief Get how the samples of the file map onto physical space.
	 *
	 * @return const image_metadata& The metadata. States nothing for a file
	 * that does not carry it.
	 */
	virtual const image_metadata& get_metadata() const noexcept = 0;

	/**
	 * @brief Start reading a set of hyperrectangles of the file into one
	 * array.
	 *
	 * The file is the file side of the plan and @p destination its array
	 * side: every region names where it starts in the file and where it
	 * lands in @p destination, and all of them share the extents the
	 * plan carries. Passing every region in one call is what lets a
	 * reader sort the regions by their position on the storage and merge
	 * neighbouring ones into a single larger read, which one call per region
	 * can not express; it also pays for the destination's geometry once
	 * rather than once per region.
	 *
	 * @p destination is the whole array, not a view of the part a region
	 * lands in, and it may be strided. The file offsets of the plan must have
	 * the rank of the file and its array offsets the rank of
	 * @p destination; a side whose rank exceeds that of the extents spans a
	 * single position along the axes they do not reach.
	 *
	 * Regions may be read in any order. If two of them land on the same
	 * elements of @p destination, which one prevails is unspecified and thus
	 * this pattern should be avoided.
	 *
	 * Values are converted to the data type of @p destination as a cast
	 * between the two types does, which preserves the numeric value where
	 * the destination can hold it.
	 * Nothing is scaled, normalised or clipped, whatever statistics the file
	 * may carry in its header; asking for the data type of
	 * @ref get_descriptor converts nothing at all.
	 *
	 * The read may still be under way when this returns. It reports to
	 * @p done exactly once: when every region has landed, or with what made
	 * it fail. It keeps @p destination, and with it the memory it writes
	 * into, until then.
	 *
	 * @par Thread safety
	 * This method may be called concurrently on one reader. A reader that
	 * can not decode in parallel serialises the calls itself, so issuing
	 * them at once never costs correctness.
	 *
	 * An empty plan reads nothing and reports success.
	 *
	 * @param destination Where the regions are written. Must be
	 * initialized.
	 * @param regions The regions to read and where each one lands.
	 * @param done Where the outcome is reported. Must not be null.
	 * @throws std::invalid_argument If @p done is null. Every other failure
	 * is reported to @p done rather than thrown:
	 * - std::invalid_argument if the file offsets do not have the rank of
	 *   the file, if the array offsets do not have the rank of
	 *   @p destination, or if @p destination is not initialized;
	 * - std::out_of_range if a region is not contained in the file, or does
	 *   not fit in @p destination where it is placed;
	 * - unsupported_operation_error if the data type of @p destination can
	 *   not be produced from the one of the file;
	 * - image_file_format_error if the file turns out to be malformed or
	 *   truncated where a region is read;
	 * - image_file_error if the file can not be read where a region is
	 *   read.
	 */
	virtual void read_async(
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) const = 0;
};

} // namespace vitrio
