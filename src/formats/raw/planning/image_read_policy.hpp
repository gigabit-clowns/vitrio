// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>

namespace vitrio
{

/**
 * @brief How the regions of a read are gathered into stretches of the file:
 * which gaps between them are bridged, how many bytes one step takes, and
 * the grid every stretch starts on.
 *
 * A stretch starts on the grid, at or before the first byte its first region
 * reaches: on a page where it is advised, so that the advice starts where
 * the system takes it, and on an element of the values where it is read
 * into memory of its own, so that the values land aligned there.
 */
class image_read_policy
{
public:
	/**
	 * @brief Construct a policy from what it settles.
	 *
	 * @param gap_tolerance Widest gap between two stretches that still merges
	 * them.
	 * @param byte_budget Most bytes one step takes, which it exceeds only
	 * where a single region is wider than it.
	 * @param alignment Spacing of the grid every stretch starts on, in
	 * bytes, which must not be zero.
	 * @param alignment_origin Offset in the file of a line of the grid, which
	 * must be below @p alignment.
	 */
	image_read_policy(
		std::size_t gap_tolerance,
		std::size_t byte_budget,
		std::size_t alignment,
		std::size_t alignment_origin
	) noexcept;

	image_read_policy(const image_read_policy &other) = default;
	image_read_policy(image_read_policy &&other) noexcept = default;
	~image_read_policy() = default;

	image_read_policy& operator=(const image_read_policy &other) = default;
	image_read_policy& operator=(image_read_policy &&other) noexcept = default;

	/**
	 * @brief Get the widest gap that still merges two stretches.
	 *
	 * @return std::size_t The gap, in bytes.
	 */
	std::size_t get_gap_tolerance() const noexcept;

	/**
	 * @brief Get the most bytes one step takes.
	 *
	 * @return std::size_t The budget, in bytes.
	 */
	std::size_t get_byte_budget() const noexcept;

	/**
	 * @brief Get the spacing of the grid every stretch starts on.
	 *
	 * @return std::size_t The spacing, in bytes.
	 */
	std::size_t get_alignment() const noexcept;

	/**
	 * @brief Get where the grid is anchored.
	 *
	 * @return std::size_t The offset in the file of a line of the grid,
	 * below its spacing.
	 */
	std::size_t get_alignment_origin() const noexcept;

private:
	std::size_t m_gap_tolerance;
	std::size_t m_byte_budget;
	std::size_t m_alignment;
	std::size_t m_alignment_origin;
};

/**
 * @brief Work out the policy the advice of a mapped read takes for regions
 * of a given span.
 *
 * The tolerance scales with the region rather than being flat, so that a gap
 * wider than the data on either side of it is never bridged; it is floored at
 * a page, below which two stretches would be advised as the same page twice,
 * and capped at @ref get_prefetch_gap_cap. Its steps advise
 * @ref get_prefetch_byte_budget bytes. Stretches start on the pages
 * of the machine, counted from the first byte of the file, which is where a
 * mapping of it starts.
 *
 * @param region_span What one region spans, in bytes.
 * @return image_read_policy The policy.
 */
image_read_policy make_prefetch_policy(std::size_t region_span) noexcept;

} // namespace vitrio
