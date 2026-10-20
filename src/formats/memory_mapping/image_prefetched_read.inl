// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_prefetched_read.hpp"

#include "image_prefetch_policy.hpp"
#include "image_prefetch_schedule.hpp"

#include <formats/strided_transfer/image_region_read_walk.hpp>
#include <formats/strided_transfer/image_region_transfer.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>

#include <array/array_data.hpp>
#include <assert.hpp>

#include <cstddef>

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

	const image_prefetch_schedule schedule(
		regions,
		values.get_strides(),
		data_type,
		walk.get_offsets().get_file(),
		memory,
		data_offset,
		make_prefetch_policy(
			compute_region_span(regions, values.get_strides(), data_type)
		)
	);

	const auto step_count = schedule.get_step_count();
	if (step_count > 0)
	{
		prefetch(schedule.get_step_ranges(0));
	}

	for (std::size_t step = 0; step < step_count; ++step)
	{
		// The step after this one is asked for before this one is walked, so
		// that it is on its way while these values are being moved.
		const auto next_step = step + 1;
		if (next_step < step_count)
		{
			prefetch(schedule.get_step_ranges(next_step));
		}

		read_regions(
			walk,
			schedule.get_step_first_region(step),
			schedule.get_step_region_count(step),
			array_data,
			target.get_data_type(),
			memory.data() + data_offset,
			data_type,
			order
		);
	}
}

} // namespace vitrio
