// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_queued_read.hpp"

#include <formats/read_planning/image_read_policy.hpp>
#include <formats/read_planning/image_read_schedule.hpp>
#include <formats/strided_transfer/image_region_read_walk.hpp>
#include <formats/strided_transfer/image_region_transfer.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/byte.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <array/array_data.hpp>
#include <memory/byte_order.hpp>
#include <system/file_read_completion.hpp>
#include <system/file_read_queue.hpp>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <memory>
#include <system_error>
#include <vector>

namespace vitrio
{

namespace
{

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

// Fails unless a read got every byte it was asked for.
void check_read(
	const std::string &path,
	const file_read_completion &completion,
	std::size_t size
)
{
	if (completion.get_error())
	{
		throw std::system_error(completion.get_error());
	}

	if (completion.get_size() < size)
	{
		throw image_file_format_error(
			path + ": read_queued_regions: The file ends before the values "
			"it is said to hold."
		);
	}
}

// Waits for the reads a read leaves outstanding as it throws, discarding how
// they end: the memory they read into must outlive them.
void wait_out(file_read_queue &queue, std::size_t outstanding) noexcept
{
	while (outstanding > 0)
	{
		try
		{
			queue.wait();
			--outstanding;
		}
		catch (const std::system_error &error)
		{
			// The system is short of memory for the reads that are queued,
			// and may have it later. Failing otherwise, it could still write
			// into their buffers, which nothing could then release safely.
			const auto code = error.code();
			if (
				code != std::errc::resource_unavailable_try_again &&
				code != std::errc::device_or_resource_busy
			)
			{
				std::terminate();
			}
		}
	}
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
	const auto element_size =
		get_size(layout.get_descriptor().get_data_type());
	const auto element_count = walk.get_layout().compute_element_count();
	const auto region_size = element_count * element_size;

	const auto &offsets = walk.get_offsets();
	const auto file_offsets = offsets.get_file();
	const auto array_offsets = offsets.get_array();
	const auto region_count = offsets.get_region_count();

	std::vector<std::size_t> run_sizes;
	std::vector<memory_range> buffers;
	std::size_t outstanding = 0;
	try
	{
		for (std::size_t first = 0; first < region_count;)
		{
			auto last = first + 1;
			while (
				last < region_count &&
				file_offsets[last] == file_offsets[last - 1] +
					static_cast<std::ptrdiff_t>(element_count)
			)
			{
				++last;
			}

			buffers.clear();
			for (auto region = first; region < last; ++region)
			{
				buffers.emplace_back(
					array_data + array_offsets[region] *
						static_cast<std::ptrdiff_t>(element_size),
					region_size
				);
			}

			const auto offset = layout.get_data_offset() +
				static_cast<std::size_t>(file_offsets[first]) * element_size;
			queue.queue(file, offset, make_span(buffers), run_sizes.size());
			++outstanding;
			run_sizes.push_back((last - first) * region_size);
			first = last;
		}

		while (outstanding > 0)
		{
			const auto completion = queue.wait();
			--outstanding;
			check_read(path, completion, run_sizes[completion.get_tag()]);
		}
	}
	catch (...)
	{
		wait_out(queue, outstanding);
		throw;
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
	const auto strides = layout.get_strides();

	// A gap wider than the regions on either side of it is not worth reading
	// to save a read.
	const auto region_span = compute_region_span(regions, strides, data_type);
	const image_read_schedule schedule(
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

	const auto ranges = schedule.get_ranges();
	const auto regions_per_range = schedule.get_regions_per_range();
	const auto step_count = schedule.get_step_count();

	// The first region of each stretch, and the step it falls in.
	std::vector<std::size_t> first_regions(ranges.size());
	std::vector<std::size_t> range_steps(ranges.size());
	std::size_t region = 0;
	for (std::size_t range = 0; range < ranges.size(); ++range)
	{
		first_regions[range] = region;
		region += regions_per_range[range];
	}
	for (std::size_t step = 0; step < step_count; ++step)
	{
		const auto first = schedule.get_step_first_range(step);
		const auto count = schedule.get_step_ranges(step).size();
		std::fill_n(range_steps.begin() + first, count, step);
	}

	std::vector<std::unique_ptr<byte[]>> buffers(ranges.size());
	std::vector<std::size_t> unmoved(step_count, 0);
	std::size_t outstanding = 0;

	const auto read_step = [&](std::size_t step)
	{
		const auto first = schedule.get_step_first_range(step);
		const auto count = schedule.get_step_ranges(step).size();
		for (auto range = first; range < first + count; ++range)
		{
			const auto size = ranges[range].get_size();
			buffers[range].reset(new byte[size]);
			const memory_range buffer(buffers[range].get(), size);
			queue.queue(
				file,
				ranges[range].get_offset(),
				make_span(&buffer, 1),
				range
			);
			++outstanding;
			++unmoved[step];
		}
	};

	// The memory the reads go into outlives them, also when a read throws.
	try
	{
		std::size_t read = 0;
		std::size_t moved = 0;
		while (moved < step_count)
		{
			// The step after the one being moved is on its way meanwhile.
			while (read < step_count && read < moved + 2)
			{
				read_step(read++);
			}

			if (unmoved[moved] == 0)
			{
				++moved;
				continue;
			}

			const auto completion = queue.wait();
			--outstanding;
			const auto range = static_cast<std::size_t>(completion.get_tag());
			check_read(path, completion, ranges[range].get_size());

			const auto origin =
				(ranges[range].get_offset() - data_offset) / element_size;
			read_regions(
				walk,
				first_regions[range],
				regions_per_range[range],
				array_data,
				array_type,
				buffers[range].get(),
				static_cast<std::ptrdiff_t>(origin),
				data_type,
				layout.get_byte_order()
			);

			buffers[range].reset();
			--unmoved[range_steps[range]];
		}
	}
	catch (...)
	{
		wait_out(queue, outstanding);
		throw;
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
