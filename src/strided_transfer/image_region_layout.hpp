// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/span.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

class image_transfer_plan;

/**
 * @brief The space every region of a plan is walked in.
 *
 * Every region of a plan has the same extents and differs only in where it
 * starts, so all of them are walked in one and the same way. This is that
 * way, worked out once: the axes to walk, innermost first, and how far
 * apart consecutive elements are along each of them on either side.
 *
 * The axes are not those of the plan. They are ordered so that the walk is
 * sequential in the side being written, the destination: a scattered write
 * through a mapping dirties its pages out of order, and what that costs on
 * the way back to the storage has no equivalent on the read side. The source
 * only breaks the ties the destination leaves. Axes that both sides walk
 * contiguously are then merged, so that the innermost one is as long as it
 * can be.
 */
class image_region_layout
{
public:
	/**
	 * @brief One axis of the walk: how many elements it has and how far
	 * apart they are on either side.
	 */
	class axis
	{
	public:
		/**
		 * @brief Construct an axis from its components.
		 *
		 * @param extent Number of elements along the axis.
		 * @param destination_stride Distance between consecutive elements of
		 * the destination, in elements.
		 * @param source_stride Distance between consecutive elements of the
		 * source, in elements.
		 */
		axis(
			std::size_t extent,
			std::ptrdiff_t destination_stride,
			std::ptrdiff_t source_stride
		) noexcept;

		/**
		 * @brief Get the number of elements along the axis.
		 *
		 * @return std::size_t The extent.
		 */
		std::size_t get_extent() const noexcept;

		/**
		 * @brief Get the distance between consecutive elements of the
		 * destination.
		 *
		 * @return std::ptrdiff_t The stride, in elements.
		 */
		std::ptrdiff_t get_destination_stride() const noexcept;

		/**
		 * @brief Get the distance between consecutive elements of the
		 * source.
		 *
		 * @return std::ptrdiff_t The stride, in elements.
		 */
		std::ptrdiff_t get_source_stride() const noexcept;

	private:
		std::size_t m_extent;
		std::ptrdiff_t m_destination_stride;
		std::ptrdiff_t m_source_stride;
	};

	/**
	 * @brief Work out the space the regions of a plan are walked in.
	 *
	 * Both sides are given as the strides of the whole side. The extents of
	 * a plan cover only its trailing axes, so only that many strides are
	 * taken.
	 *
	 * @param regions The regions whose extents the space has.
	 * @param destination_strides Strides of the side being written, in
	 * elements.
	 * @param source_strides Strides of the side being read, in elements.
	 */
	image_region_layout(
		const image_transfer_plan &regions,
		span<const std::ptrdiff_t> destination_strides,
		span<const std::ptrdiff_t> source_strides
	);

	/**
	 * @brief Get how many axes the walk has.
	 *
	 * @return std::size_t The number of axes. It may be lower than the rank
	 * of the regions, and is zero when a region is a single element.
	 */
	std::size_t get_axis_count() const noexcept;

	/**
	 * @brief Get one axis of the walk.
	 *
	 * @param index Index of the axis, counted from the innermost one. Must
	 * be below @ref get_axis_count.
	 * @return const axis& The axis. It refers to storage owned by this
	 * layout.
	 */
	const axis& get_axis(std::size_t index) const noexcept;

	/**
	 * @brief Get how many elements one region has.
	 *
	 * @return std::size_t The product of the extents of the axes.
	 */
	std::size_t compute_element_count() const noexcept;

private:
	std::vector<axis> m_axes;

	void sort_axes_by_locality() noexcept;
	void merge_contiguous_axes();
};

} // namespace vitrio
