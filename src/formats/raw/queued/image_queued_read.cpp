// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_queued_read.hpp"

#include "file_read_plan.hpp"
#include "file_read_plan_execution.hpp"

#include <formats/raw/planning/image_read_policy.hpp>
#include <formats/raw/planning/image_read_schedule.hpp>
#include <strided_transfer/image_region_read_walk.hpp>
#include <strided_transfer/image_region_transfer.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <array/array_data.hpp>
#include <memory/byte_order.hpp>
#include <system/file_read_queue.hpp>

#include <algorithm>
#include <cstddef>
#include <system_error>
#include <vector>

namespace vitrio
{

namespace
{

// Where staged stretches start, so that the values of every data type land
// aligned in them.
constexpr std::size_t staging_alignment = 64;

// Where each stretch of a schedule is staged, within the half of the staging
// memory its step takes, and how many bytes a half takes.
struct staging_layout
{
	std::vector<std::size_t> offsets;
	std::size_t half;
};

// Whether the regions lie in the file as they are to lie in the array: one
// value after another on both sides, in the data type and byte order the
// array takes.
bool lie_as_they_land(
	const image_region_read_walk &walk,
	numerical_type array_type,
	const raw_image_layout &layout
) noexcept
{
	if (
		array_type != layout.get_descriptor().get_data_type() ||
		layout.get_byte_order() != get_system_byte_order()
	)
	{
		return false;
	}

	// The walk merges the axes both sides lay out one value after another,
	// so regions that do are left with one axis at most, along which both
	// sides step a value at a time.
	const auto &space = walk.get_layout();
	switch (space.get_axis_count())
	{
	case 0:
		return true;
	case 1:
		return
			space.get_axis(0).get_destination_stride() == 1 &&
			space.get_axis(0).get_source_stride() == 1;
	default:
		return false;
	}
}

// The end of the run of regions from `first` on that follow one another in
// the file.
std::size_t find_run_end(
	span<const std::ptrdiff_t> file_offsets,
	std::size_t first,
	std::size_t element_count
) noexcept
{
	const auto region_length = static_cast<std::ptrdiff_t>(element_count);

	auto last = first + 1;
	while (
		last < file_offsets.size() &&
		file_offsets[last] == file_offsets[last - 1] + region_length
	)
	{
		++last;
	}

	return last;
}

// The reads of regions that lie as they land: straight into the array, the
// regions that follow one another in the file as one read, all in one step.
file_read_plan plan_direct_reads(
	const raw_image_layout &layout,
	const image_region_read_walk &walk,
	byte *array_data
)
{
	const auto element_size =
		get_size(layout.get_descriptor().get_data_type());
	const auto element_count = walk.get_layout().compute_element_count();
	const auto region_size = element_count * element_size;

	const auto &offsets = walk.get_offsets();
	const auto file_offsets = offsets.get_file();
	const auto array_offsets = offsets.get_array();

	file_read_plan plan;
	std::vector<memory_range> buffers;
	for (std::size_t first = 0; first < file_offsets.size();)
	{
		const auto last = find_run_end(file_offsets, first, element_count);

		buffers.clear();
		for (auto region = first; region < last; ++region)
		{
			const auto array_offset = array_offsets[region] *
				static_cast<std::ptrdiff_t>(element_size);
			buffers.emplace_back(array_data + array_offset, region_size);
		}

		const auto file_offset =
			static_cast<std::size_t>(file_offsets[first]) * element_size;
		plan.add_read(
			0,
			layout.get_data_offset() + file_offset,
			make_span(buffers)
		);

		first = last;
	}

	return plan;
}

// The stretches of the file the regions reach, merged across the gaps that
// are narrower than a region, on the grid of the elements of the values.
image_read_schedule schedule_staged_stretches(
	const image_transfer_plan &regions,
	const image_region_read_walk &walk,
	const raw_image_layout &layout,
	std::size_t file_size,
	std::size_t byte_budget
)
{
	const auto data_type = layout.get_descriptor().get_data_type();
	const auto element_size = get_size(data_type);
	const auto data_offset = layout.get_data_offset();
	const auto strides = layout.get_strides();
	const auto region_span = compute_region_span(regions, strides, data_type);

	return image_read_schedule(
		regions,
		strides,
		data_type,
		walk.get_offsets().get_file(),
		data_offset,
		file_size,
		image_read_policy(
			std::min(region_span, default_prefetch_gap_cap),
			byte_budget,
			element_size,
			data_offset % element_size
		)
	);
}

// The first region of each stretch: the regions of a stretch follow those of
// the one before it.
std::vector<std::size_t>
find_first_regions(const image_read_schedule &schedule)
{
	const auto regions_per_range = schedule.get_regions_per_range();

	std::vector<std::size_t> first_regions(regions_per_range.size());
	std::size_t region = 0;
	for (std::size_t range = 0; range < first_regions.size(); ++range)
	{
		first_regions[range] = region;
		region += regions_per_range[range];
	}

	return first_regions;
}

staging_layout lay_out_staging(const image_read_schedule &schedule)
{
	const auto ranges = schedule.get_ranges();

	staging_layout layout{std::vector<std::size_t>(ranges.size()), 0};
	for (std::size_t step = 0; step < schedule.get_step_count(); ++step)
	{
		const auto first = schedule.get_step_first_range(step);
		const auto count = schedule.get_step_ranges(step).size();

		std::size_t offset = 0;
		for (auto range = first; range < first + count; ++range)
		{
			layout.offsets[range] = offset;
			const auto size = ranges[range].get_size();
			offset += (size + staging_alignment - 1) / staging_alignment *
				staging_alignment;
		}

		layout.half = std::max(layout.half, offset);
	}

	return layout;
}

// The reads of the stretches of a schedule into staging memory, one read a
// stretch. A step is read while the one before it is moved, so the steps
// take turns at two halves of the memory, which this sizes.
file_read_plan plan_staged_reads(
	const image_read_schedule &schedule,
	std::vector<byte> &staging
)
{
	const auto ranges = schedule.get_ranges();
	const auto step_count = schedule.get_step_count();
	const auto layout = lay_out_staging(schedule);

	staging.resize(layout.half * std::min<std::size_t>(step_count, 2));

	file_read_plan plan;
	for (std::size_t step = 0; step < step_count; ++step)
	{
		const auto first = schedule.get_step_first_range(step);
		const auto count = schedule.get_step_ranges(step).size();
		auto *half = staging.data() + (step % 2) * layout.half;

		for (auto range = first; range < first + count; ++range)
		{
			const memory_range buffer(
				half + layout.offsets[range],
				ranges[range].get_size()
			);
			plan.add_read(
				step,
				ranges[range].get_offset(),
				make_span(&buffer, 1)
			);
		}
	}

	return plan;
}

void read_directly(
	const std::string &path,
	file_handle_t file,
	const raw_image_layout &layout,
	const image_region_read_walk &walk,
	byte *array_data,
	file_read_queue &queue
)
{
	const auto plan = plan_direct_reads(layout, walk, array_data);

	file_read_plan_execution execution(path, file, plan, queue);
	std::size_t read = 0;
	while (execution.take_next(read))
	{
		// The values of the stretch landed where they belong.
	}
}

void read_staged(
	const std::string &path,
	file_handle_t file,
	std::size_t file_size,
	const raw_image_layout &layout,
	const image_transfer_plan &regions,
	const image_region_read_walk &walk,
	byte *array_data,
	numerical_type array_type,
	file_read_queue &queue,
	std::size_t byte_budget
)
{
	const auto data_type = layout.get_descriptor().get_data_type();
	const auto element_size = get_size(data_type);
	const auto data_offset = layout.get_data_offset();

	const auto schedule = schedule_staged_stretches(
		regions,
		walk,
		layout,
		file_size,
		byte_budget
	);
	const auto first_regions = find_first_regions(schedule);
	const auto regions_per_range = schedule.get_regions_per_range();

	std::vector<byte> staging;
	const auto plan = plan_staged_reads(schedule, staging);

	// One read a stretch, so a read and its stretch share their index.
	file_read_plan_execution execution(path, file, plan, queue);
	std::size_t range = 0;
	while (execution.take_next(range))
	{
		const auto *staged = static_cast<const byte*>(
			plan.get_buffers(range)[0].get_address()
		);
		const auto origin =
			(plan.get_offset(range) - data_offset) / element_size;

		read_regions(
			walk,
			first_regions[range],
			regions_per_range[range],
			array_data,
			array_type,
			staged,
			static_cast<std::ptrdiff_t>(origin),
			data_type,
			layout.get_byte_order()
		);
	}
}

} // anonymous namespace

void read_queued_regions(
	const std::string &path,
	file_handle_t file,
	std::size_t file_size,
	const raw_image_layout &layout,
	array_ref destination,
	const image_transfer_plan &regions,
	file_read_queue &queue,
	std::size_t byte_budget
)
{
	auto *array_data = get_array_data(destination);
	const auto &target = destination.get_descriptor();

	const image_region_read_walk walk(
		regions,
		layout.get_descriptor().get_extents(),
		layout.get_strides(),
		target.get_extents(),
		target.get_strides(),
		target.get_offset()
	);

	if (
		walk.get_offsets().get_region_count() == 0 ||
		walk.get_layout().compute_element_count() == 0
	)
	{
		return;
	}

	try
	{
		if (lie_as_they_land(walk, target.get_data_type(), layout))
		{
			read_directly(path, file, layout, walk, array_data, queue);
		}
		else
		{
			read_staged(
				path,
				file,
				file_size,
				layout,
				regions,
				walk,
				array_data,
				target.get_data_type(),
				queue,
				byte_budget
			);
		}
	}
	catch (const std::system_error &error)
	{
		throw image_file_error(
			path + ": read_queued_regions: The file could not be read: " +
			error.what()
		);
	}
}

} // namespace vitrio
