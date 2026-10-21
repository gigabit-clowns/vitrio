// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_prefetched_read.hpp"

#include <formats/read_planning/image_read_policy.hpp>
#include <formats/read_planning/image_read_schedule.hpp>
#include <formats/strided_transfer/image_region_read_walk.hpp>
#include <formats/strided_transfer/image_region_transfer.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>

#include <array/array_data.hpp>
#include <assert.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

template <typename Prefetch>
inline void read_prefetched_regions(
	span<const byte> memory,
	const array_descriptor &values,
	byte_order order,
	array_ref destination,
	const image_transfer_plan &regions,
	Prefetch &&prefetch
)
{
	VITRIO_ASSERT(values.get_offset() >= 0);

	auto *array_data = get_array_data(destination);
	const auto &target = destination.get_descriptor();
	const auto data_type = values.get_data_type();
	const auto data_offset =
		static_cast<std::size_t>(values.get_offset()) * get_size(data_type);

	const image_region_read_walk walk(
		regions,
		values.get_extents(),
		values.get_strides(),
		target.get_extents(),
		target.get_strides(),
		target.get_offset()
	);

	const image_read_schedule schedule(
		regions,
		values.get_strides(),
		data_type,
		walk.get_offsets().get_file(),
		data_offset,
		memory.size(),
		make_prefetch_policy(
			compute_region_span(regions, values.get_strides(), data_type)
		)
	);

	// The schedule names stretches of the file, which lie at the same
	// offsets in the memory that holds it.
	std::vector<memory_range> stretches;
	const auto ask_for = [&](std::size_t step)
	{
		stretches.clear();
		for (const auto &range : schedule.get_step_ranges(step))
		{
			stretches.emplace_back(
				const_cast<byte*>(memory.data()) + range.get_offset(),
				range.get_size()
			);
		}
		prefetch(make_span(stretches.data(), stretches.size()));
	};

	const auto step_count = schedule.get_step_count();
	if (step_count > 0)
	{
		ask_for(0);
	}

	for (std::size_t step = 0; step < step_count; ++step)
	{
		// The step after this one is asked for before this one is walked, so
		// that it is on its way while these values are being moved.
		const auto next_step = step + 1;
		if (next_step < step_count)
		{
			ask_for(next_step);
		}

		read_regions(
			walk,
			schedule.get_step_first_region(step),
			schedule.get_step_region_count(step),
			array_data,
			target.get_data_type(),
			memory.data() + data_offset,
			0,
			data_type,
			order
		);
	}
}

} // namespace vitrio
