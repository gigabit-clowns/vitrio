// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_region_split.hpp"

#include <vitrio/image_transfer_shape.hpp>

#include <assert.hpp>

#include <algorithm>
#include <numeric>
#include <vector>

namespace vitrio
{

namespace
{

// The axes of a region to split it along: those the gathering from the
// fastest axis stops at, and every slower one.
std::vector<std::size_t> find_split_axes(
	const image_transfer_shape &shape,
	span<const std::ptrdiff_t> file_strides,
	std::size_t element_size,
	std::size_t page_size,
	std::size_t gap_cap,
	std::size_t byte_budget
)
{
	const auto extents = shape.get_extents();
	const auto rank = extents.size();
	const auto lead = shape.get_file_rank() - rank;

	// The axes a region steps along, fastest in the file first.
	std::vector<std::size_t> axes;
	for (std::size_t axis = 0; axis < rank; ++axis)
	{
		if (extents[axis] > 1)
		{
			axes.push_back(axis);
		}
	}
	const auto stride_of = [&](std::size_t axis)
	{
		VITRIO_ASSERT(file_strides[lead + axis] >= 0);
		return static_cast<std::size_t>(file_strides[lead + axis]) *
			element_size;
	};
	std::stable_sort(
		axes.begin(),
		axes.end(),
		[&](std::size_t a, std::size_t b)
		{
			return stride_of(a) < stride_of(b);
		}
	);

	std::size_t span = element_size;
	for (std::size_t index = 0; index < axes.size(); ++index)
	{
		const auto axis = axes[index];
		const auto stride = stride_of(axis);
		const auto joined = (extents[axis] - 1) * stride + span;
		if (stride <= span)
		{
			// No gap: the parts gathered so far follow one another, or
			// overlap, along this axis.
			span = joined;
			continue;
		}

		const auto gap = stride - span;
		const auto tolerance = std::min(std::max(span, page_size), gap_cap);
		if (gap <= tolerance && joined <= byte_budget)
		{
			span = joined;
			continue;
		}

		return std::vector<std::size_t>(axes.begin() + index, axes.end());
	}

	return {};
}

} // anonymous namespace

image_transfer_plan split_scattered_regions(
	const image_transfer_plan &regions,
	span<const std::ptrdiff_t> file_strides,
	numerical_type data_type,
	std::size_t page_size,
	std::size_t gap_cap,
	std::size_t byte_budget
)
{
	const auto &shape = regions.get_shape();
	VITRIO_ASSERT(file_strides.size() == shape.get_file_rank());

	const auto split_axes = find_split_axes(
		shape,
		file_strides,
		get_size(data_type),
		page_size,
		gap_cap,
		byte_budget
	);
	if (split_axes.empty())
	{
		return regions;
	}

	const auto extents = shape.get_extents();
	const auto rank = extents.size();
	const auto file_lead = shape.get_file_rank() - rank;
	const auto array_lead = shape.get_array_rank() - rank;

	std::vector<std::size_t> part_extents(extents.begin(), extents.end());
	std::size_t parts_per_region = 1;
	for (const auto axis : split_axes)
	{
		parts_per_region *= extents[axis];
		part_extents[axis] = 1;
	}

	image_transfer_plan parts(
		image_transfer_shape(
			std::move(part_extents),
			shape.get_file_rank(),
			shape.get_array_rank()
		)
	);
	parts.reserve(regions.get_region_count() * parts_per_region);

	std::vector<std::size_t> file_offset;
	std::vector<std::size_t> array_offset;
	std::vector<std::size_t> position(split_axes.size(), 0);
	for (std::size_t region = 0; region < regions.get_region_count(); ++region)
	{
		const auto file_origin = regions.get_file_offset(region);
		const auto array_origin = regions.get_array_offset(region);
		std::fill(position.begin(), position.end(), 0);
		for (std::size_t part = 0; part < parts_per_region; ++part)
		{
			file_offset.assign(file_origin.begin(), file_origin.end());
			array_offset.assign(array_origin.begin(), array_origin.end());
			for (std::size_t index = 0; index < split_axes.size(); ++index)
			{
				const auto axis = split_axes[index];
				file_offset[file_lead + axis] += position[index];
				array_offset[array_lead + axis] += position[index];
			}
			parts.add(make_span(file_offset), make_span(array_offset));

			// The next position, the last split axis counting fastest.
			for (auto index = split_axes.size(); index-- > 0;)
			{
				if (++position[index] < extents[split_axes[index]])
				{
					break;
				}
				position[index] = 0;
			}
		}
	}

	return parts;
}

} // namespace vitrio
