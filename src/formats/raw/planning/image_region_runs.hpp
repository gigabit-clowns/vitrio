// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

class image_transfer_plan;

/**
 * @brief The runs a region of a plan lays in a file: the stretches of
 * consecutive bytes it covers there.
 *
 * Every region of a plan shares the extents, so all of them lay the same
 * runs, each from where it starts. A region whose elements follow one another
 * in the file is one run. One that cuts across an axis the file lays out
 * faster than another, such as a patch of an image, is a run for each row it
 * takes.
 *
 * A write covers its runs and nothing between them, which it would
 * overwrite.
 */
class image_region_runs
{
public:
	/**
	 * @brief Work out the runs of the regions of a plan.
	 *
	 * @param regions The regions.
	 * @param file_strides Distance between consecutive elements of the file
	 * along each axis, in elements. None is negative.
	 * @param data_type Data type of the elements of the file.
	 */
	image_region_runs(
		const image_transfer_plan &regions,
		span<const std::ptrdiff_t> file_strides,
		numerical_type data_type
	);

	image_region_runs(const image_region_runs &other) = default;
	image_region_runs(image_region_runs &&other) noexcept = default;
	~image_region_runs() = default;

	image_region_runs& operator=(const image_region_runs &other) = default;
	image_region_runs&
	operator=(image_region_runs &&other) noexcept = default;

	/**
	 * @brief Get how many bytes each run covers.
	 *
	 * @return std::size_t The size in bytes. Zero where a region has no
	 * element.
	 */
	std::size_t get_run_size() const noexcept;

	/**
	 * @brief Get where each run starts.
	 *
	 * @return span<const std::size_t> The offsets, in bytes from where the
	 * region starts, ascending. Empty where a region has no element.
	 */
	span<const std::size_t> get_run_offsets() const noexcept;

private:
	std::size_t m_run_size;
	std::vector<std::size_t> m_run_offsets;
};

} // namespace vitrio
