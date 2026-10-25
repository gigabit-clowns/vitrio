// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_region_runs.hpp"

#include <vitrio/image_transfer_plan.hpp>

#include <assert.hpp>

#include <algorithm>

namespace vitrio
{

namespace
{

struct run_axis
{
	std::size_t extent;
	std::size_t stride;
};

} // anonymous namespace

image_region_runs::image_region_runs(
	const image_transfer_plan &regions,
	span<const std::ptrdiff_t> file_strides,
	numerical_type data_type
)
	: m_run_size(0)
{
	const auto &shape = regions.get_shape();
	const auto rank = file_strides.size();

	// An axis a region takes one element of lays no run of its own.
	std::vector<run_axis> axes;
	for (std::size_t axis = 0; axis < rank; ++axis)
	{
		const auto extent = shape.get_extent(rank, axis);
		if (extent == 0)
		{
			return;
		}

		VITRIO_ASSERT(file_strides[axis] >= 0);
		if (extent > 1)
		{
			axes.push_back(
				{extent, static_cast<std::size_t>(file_strides[axis])}
			);
		}
	}

	std::sort(
		axes.begin(),
		axes.end(),
		[](const run_axis &a, const run_axis &b)
		{
			return a.stride < b.stride;
		}
	);

	// The fastest axes the file lays out one after another, each taken
	// whole by the region before the next, make up a run.
	std::size_t run_length = 1;
	std::size_t first_outer = 0;
	while (
		first_outer < axes.size() &&
		axes[first_outer].stride == run_length
	)
	{
		run_length *= axes[first_outer].extent;
		++first_outer;
	}

	const auto element_size = get_size(data_type);
	m_run_size = run_length * element_size;

	// Every combination of positions along the other axes starts a run.
	m_run_offsets.push_back(0);
	for (auto axis = first_outer; axis < axes.size(); ++axis)
	{
		const auto count = m_run_offsets.size();
		const auto step = axes[axis].stride * element_size;
		for (std::size_t position = 1; position < axes[axis].extent; ++position)
		{
			for (std::size_t run = 0; run < count; ++run)
			{
				m_run_offsets.push_back(m_run_offsets[run] + position * step);
			}
		}
	}

	std::sort(m_run_offsets.begin(), m_run_offsets.end());
}

std::size_t image_region_runs::get_run_size() const noexcept
{
	return m_run_size;
}

span<const std::size_t> image_region_runs::get_run_offsets() const noexcept
{
	return make_span(m_run_offsets);
}

} // namespace vitrio
