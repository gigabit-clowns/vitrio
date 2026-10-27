// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "file_range.hpp"
#include "image_transfer_policy.hpp"

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

class image_transfer_plan;

/**
 * @brief Work out what each region of a plan spans, in bytes.
 *
 * The stretch from the first byte of a region to its last, gaps included,
 * which is what a region occupies when it is not contiguous. Every region of
 * a plan shares the extents, so all of them span the same.
 *
 * @param regions The regions to be moved.
 * @param file_strides Distance between consecutive elements of the file along
 * each axis, in elements.
 * @param data_type Data type of the elements of the file.
 * @return std::size_t The span in bytes, zero for a region of no elements.
 */
std::size_t compute_region_span(
	const image_transfer_plan &regions,
	span<const std::ptrdiff_t> file_strides,
	numerical_type data_type
) noexcept;

/**
 * @brief The stretches of a file a set of regions reaches, grouped into
 * steps.
 *
 * Regions are gathered as the stretches they actually touch rather than as
 * the one span that covers them, so regions scattered over a large file reach
 * what they need and not the file. Stretches that overlap or that lie within
 * the tolerance of each other are merged, which collapses consecutive regions
 * back into the few stretches they are. Merging stops where a stretch would
 * grow past the budget of a step, so a long run of consecutive regions is
 * taken a step at a time rather than all at once.
 *
 * Every stretch starts on the grid of the policy, at or before the first
 * byte its first region reaches, and is cut short where the file ends.
 *
 * The merged stretches are grouped into steps of a bounded number of bytes,
 * so that one step can be transferred while the one before it is moved.
 * Regions that fit in the budget are one step, so small scattered regions
 * are all taken before any of them is moved, and their latencies overlap. A
 * step always holds at least one region, so a region wider than the budget is
 * taken whole.
 *
 * The steps tile the regions: every region belongs to exactly one of them,
 * so moving the steps moves every region. A region with no bytes in the
 * file, one that starts past its end, still belongs to a step: this decides
 * what is transferred, not what is moved.
 *
 * The file offsets it is built from must be ascending.
 */
class image_transfer_schedule
{
public:
	/**
	 * @brief Work out the stretches of the regions and the steps they fall
	 * in.
	 *
	 * @param regions The regions to be moved.
	 * @param file_strides Distance between consecutive elements of the file
	 * along each axis, in elements.
	 * @param data_type Data type of the elements of the file.
	 * @param file_offsets Where each region starts among the values of the
	 * file, in elements, ascending.
	 * @param data_offset Where the values begin in the file, in bytes.
	 * @param file_size How many bytes the file has, which no stretch reaches
	 * past.
	 * @param policy How the regions are gathered.
	 */
	image_transfer_schedule(
		const image_transfer_plan &regions,
		span<const std::ptrdiff_t> file_strides,
		numerical_type data_type,
		span<const std::ptrdiff_t> file_offsets,
		std::size_t data_offset,
		std::size_t file_size,
		const image_transfer_policy &policy
	);

	image_transfer_schedule(const image_transfer_schedule &other) = default;
	image_transfer_schedule(image_transfer_schedule &&other) noexcept = default;
	~image_transfer_schedule() = default;

	image_transfer_schedule&
	operator=(const image_transfer_schedule &other) = default;
	image_transfer_schedule&
	operator=(image_transfer_schedule &&other) noexcept = default;

	/**
	 * @brief Get how many steps the regions fall in.
	 *
	 * @return std::size_t The number of steps, zero when there is no region.
	 */
	std::size_t get_step_count() const noexcept;

	/**
	 * @brief Get every stretch the regions reach.
	 *
	 * @return span<const file_range> The stretches, ascending and disjoint.
	 */
	span<const file_range> get_ranges() const noexcept;

	/**
	 * @brief Get the stretches of one step.
	 *
	 * @param step Index of the step, below @ref get_step_count.
	 * @return span<const file_range> The stretches of that step, empty where
	 * its regions have no bytes in the file.
	 */
	span<const file_range> get_step_ranges(std::size_t step) const noexcept;

	/**
	 * @brief Get the index of the first stretch of one step.
	 *
	 * @param step Index of the step, below @ref get_step_count.
	 * @return std::size_t Its index among @ref get_ranges.
	 */
	std::size_t get_step_first_range(std::size_t step) const noexcept;

	/**
	 * @brief Get the first region one step covers.
	 *
	 * @param step Index of the step, below @ref get_step_count.
	 * @return std::size_t Its index among the regions, in the order the file
	 * offsets were given in.
	 */
	std::size_t get_step_first_region(std::size_t step) const noexcept;

	/**
	 * @brief Get how many regions one step covers.
	 *
	 * @param step Index of the step, below @ref get_step_count.
	 * @return std::size_t The number of regions, never zero.
	 */
	std::size_t get_step_region_count(std::size_t step) const noexcept;

	/**
	 * @brief Get how many regions each stretch covers.
	 *
	 * @return span<const std::size_t> One count per stretch of
	 * @ref get_ranges. The regions of a stretch follow those of the one
	 * before it, in the order the file offsets were given in.
	 */
	span<const std::size_t> get_regions_per_range() const noexcept;

private:
	/**
	 * @brief Work out the stretches of the regions, merging as they are
	 * found.
	 */
	void gather_ranges(
		const image_transfer_plan &regions,
		span<const std::ptrdiff_t> file_strides,
		numerical_type data_type,
		span<const std::ptrdiff_t> file_offsets,
		std::size_t data_offset,
		std::size_t file_size,
		const image_transfer_policy &policy
	);

	/**
	 * @brief Group the stretches into steps of a bounded number of bytes.
	 *
	 * @param region_count How many regions there are, which the last
	 * step reaches whether or not every one of them has bytes in the file.
	 * @param byte_budget Most bytes one step takes.
	 */
	void gather_steps(std::size_t region_count, std::size_t byte_budget);

	std::vector<file_range> m_ranges;
	std::vector<std::size_t> m_regions_per_range;
	std::vector<std::size_t> m_step_first_range;
	std::vector<std::size_t> m_step_first_region;
};

} // namespace vitrio
