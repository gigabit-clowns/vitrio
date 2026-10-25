// SPDX-License-Identifier: LGPL-2.1-or-later

#include "make_image_read_operation.hpp"

#include "direct_image_read_operation.hpp"
#include "staged_image_read_operation.hpp"

#include <formats/raw/planning/image_transfer_policy.hpp>
#include <formats/raw/planning/image_transfer_schedule.hpp>
#include <strided_transfer/image_region_read_walk.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>

#include <array/array_data.hpp>
#include <memory/byte_order.hpp>
#include <tuning.hpp>

#include <algorithm>
#include <utility>

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

// The stretches of the file the regions reach, merged across the gaps that
// are narrower than a region, on the grid of the elements of the values.
image_transfer_schedule schedule_stretches(
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

	return image_transfer_schedule(
		regions,
		strides,
		data_type,
		walk.get_offsets().get_file(),
		data_offset,
		file_size,
		image_transfer_policy(
			std::min(region_span, get_prefetch_gap_cap()),
			byte_budget,
			element_size,
			data_offset % element_size
		)
	);
}

} // anonymous namespace

std::unique_ptr<file_transfer_operation> make_image_read_operation(
	const std::string &path,
	std::shared_ptr<const boost::interprocess::file_mapping> file,
	std::size_t file_size,
	const raw_image_layout &layout,
	array destination,
	const image_transfer_plan &regions,
	std::size_t byte_budget
)
{
	// Refuses an array that is not initialized.
	get_array_data(array_ref(destination));
	const auto &target = destination.get_descriptor();

	image_region_read_walk walk(
		regions,
		layout.get_descriptor().get_extents(),
		layout.get_strides(),
		target.get_extents(),
		target.get_strides(),
		target.get_offset()
	);

	const auto nothing_to_read =
		walk.get_offsets().get_region_count() == 0 ||
		walk.get_layout().compute_element_count() == 0;
	if (
		nothing_to_read ||
		lie_as_they_land(walk, target.get_data_type(), layout)
	)
	{
		return std::make_unique<direct_image_read_operation>(
			path,
			std::move(file),
			layout,
			std::move(destination),
			walk
		);
	}

	const auto schedule = schedule_stretches(
		regions,
		walk,
		layout,
		file_size,
		byte_budget
	);
	return std::make_unique<staged_image_read_operation>(
		path,
		std::move(file),
		layout,
		std::move(destination),
		std::move(walk),
		schedule
	);
}

} // namespace vitrio
