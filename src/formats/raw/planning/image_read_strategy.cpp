// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_read_strategy.hpp"
#include "image_transfer_policy.hpp"

#include <formats/raw/raw_image_layout.hpp>
#include <strided_transfer/image_region_read_walk.hpp>

#include <vitrio/image_descriptor.hpp>

#include <memory/byte_order.hpp>
#include <tuning.hpp>

#include <algorithm>

namespace vitrio
{

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

image_transfer_schedule schedule_staged_read(
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

} // namespace vitrio
