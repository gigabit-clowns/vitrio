// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mapped_image_reader.hpp"

#include "image_prefetch_policy.hpp"

#include <formats/strided_transfer/image_region_transfer.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <array/array_data.hpp>
#include <system/page_prefetch.hpp>

#include <cstddef>
#include <utility>

namespace vitrio
{

mapped_image_reader::mapped_image_reader(
	const std::string &path,
	image_file_layout layout,
	image_metadata metadata
)
	: m_mapping(path, image_file_access::read_only)
	, m_layout(std::move(layout))
	, m_metadata(std::move(metadata))
{
	const auto required =
		m_layout.get_data_offset() + m_layout.get_data_size();
	if (m_mapping.get_size() < required)
	{
		throw image_file_format_error(
			path + ": mapped_image_reader: The file is shorter than the "
			"values it is said to hold."
		);
	}
}

const image_descriptor& mapped_image_reader::get_descriptor() const noexcept
{
	return m_layout.get_descriptor();
}

const image_metadata& mapped_image_reader::get_metadata() const noexcept
{
	return m_metadata;
}

void mapped_image_reader::read(
	array_ref destination,
	const image_transfer_plan &regions
) const
{
	auto *array_data = get_array_data(destination);

	// The regions are resolved, and so bounds checked, before any of them
	// is asked for.
	const auto walk = resolve_regions(destination, regions);
	const auto schedule = schedule_prefetch(regions, walk);

	move_regions(
		walk,
		schedule,
		array_data,
		destination.get_descriptor().get_data_type()
	);
}

image_region_read_walk mapped_image_reader::resolve_regions(
	array_ref destination,
	const image_transfer_plan &regions
) const
{
	const auto &descriptor = destination.get_descriptor();

	return image_region_read_walk(
		regions,
		m_layout.get_descriptor().get_extents(),
		m_layout.get_strides(),
		descriptor.get_extents(),
		descriptor.get_strides(),
		descriptor.get_offset()
	);
}

image_prefetch_schedule mapped_image_reader::schedule_prefetch(
	const image_transfer_plan &regions,
	const image_region_read_walk &walk
) const
{
	const auto data_type = m_layout.get_descriptor().get_data_type();

	return image_prefetch_schedule(
		regions,
		m_layout.get_strides(),
		data_type,
		walk.get_offsets().get_file(),
		make_span(m_mapping.get_data(), m_mapping.get_size()),
		m_layout.get_data_offset(),
		make_prefetch_policy(
			compute_region_span(regions, m_layout.get_strides(), data_type)
		)
	);
}

void mapped_image_reader::move_regions(
	const image_region_read_walk &walk,
	const image_prefetch_schedule &schedule,
	void *array_data,
	numerical_type array_type
) const
{
	const auto *file_data = m_mapping.get_data() + m_layout.get_data_offset();
	const auto step_count = schedule.get_step_count();

	if (step_count > 0)
	{
		prefetch_pages(schedule.get_step_ranges(0));
	}

	for (std::size_t step = 0; step < step_count; ++step)
	{
		// The step after this one is asked for before this one is walked, so
		// that it is on its way while these values are being moved.
		const auto next_step = step + 1;
		if (next_step < step_count)
		{
			prefetch_pages(schedule.get_step_ranges(next_step));
		}

		read_regions(
			walk,
			schedule.get_step_first_region(step),
			schedule.get_step_region_count(step),
			array_data,
			array_type,
			file_data,
			m_layout.get_descriptor().get_data_type(),
			m_layout.get_byte_order()
		);
	}
}

} // namespace vitrio
