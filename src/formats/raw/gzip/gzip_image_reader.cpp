// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gzip_image_reader.hpp"

#include <compression/gzip_file_stream.hpp>
#include <formats/raw/planning/image_read_strategy.hpp>
#include <formats/raw/planning/image_region_split.hpp>
#include <formats/raw/planning/image_transfer_schedule.hpp>
#include <strided_transfer/image_region_read_walk.hpp>
#include <strided_transfer/image_region_transfer.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/image_descriptor.hpp>

#include <array/array_data.hpp>
#include <tuning.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace vitrio
{

namespace
{

// Whether some regions, of a given number of elements each and in ascending
// file order, overlap in the file. A stream reaches each byte once, so
// regions that overlap are staged, where their stretches are merged.
bool overlap(
	span<const std::ptrdiff_t> file_offsets,
	std::size_t element_count
) noexcept
{
	const auto length = static_cast<std::ptrdiff_t>(element_count);
	for (std::size_t region = 1; region < file_offsets.size(); ++region)
	{
		if (file_offsets[region] < file_offsets[region - 1] + length)
		{
			return true;
		}
	}

	return false;
}

// Inflate each region straight to where it lands in the array.
void read_direct(
	gzip_file_stream &stream,
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
	for (std::size_t region = 0; region < file_offsets.size(); ++region)
	{
		const auto start = layout.get_data_offset() +
			static_cast<std::size_t>(file_offsets[region]) * element_size;
		stream.skip(start - stream.get_position());

		const auto array_offset = array_offsets[region] *
			static_cast<std::ptrdiff_t>(element_size);
		stream.read(make_span(array_data + array_offset, region_size));
	}
}

// Inflate each stretch of the file the regions reach into staging, and
// convert its regions from there into the array.
void read_staged(
	gzip_file_stream &stream,
	const raw_image_layout &layout,
	const image_transfer_plan &regions,
	const image_region_read_walk &walk,
	byte *array_data,
	numerical_type array_type
)
{
	const auto data_offset = layout.get_data_offset();
	const auto schedule = schedule_staged_read(
		regions,
		walk,
		layout,
		data_offset + layout.get_data_size(),
		get_staging_byte_budget()
	);

	const auto ranges = schedule.get_ranges();
	const auto regions_per_range = schedule.get_regions_per_range();

	std::size_t largest = 0;
	for (const auto &range : ranges)
	{
		largest = std::max(largest, range.get_size());
	}
	std::vector<byte> staging(largest);

	const auto file_type = layout.get_descriptor().get_data_type();
	const auto element_size = get_size(file_type);
	std::size_t first_region = 0;
	for (std::size_t range = 0; range < ranges.size(); ++range)
	{
		const auto &stretch = ranges[range];
		stream.skip(stretch.get_offset() - stream.get_position());
		stream.read(make_span(staging.data(), stretch.get_size()));

		// Stretches start on the grid of the elements, at or after the
		// first of them.
		const auto origin = (stretch.get_offset() - data_offset) /
			element_size;
		read_regions(
			walk,
			first_region,
			regions_per_range[range],
			array_data,
			array_type,
			staging.data(),
			static_cast<std::ptrdiff_t>(origin),
			file_type,
			layout.get_byte_order()
		);

		first_region += regions_per_range[range];
	}
}

} // anonymous namespace

gzip_image_reader::gzip_image_reader(
	std::string path,
	raw_image_layout layout,
	image_metadata metadata
)
	: m_path(std::move(path))
	, m_layout(std::move(layout))
	, m_metadata(std::move(metadata))
{
}

const image_descriptor& gzip_image_reader::get_descriptor() const noexcept
{
	return m_layout.get_descriptor();
}

const image_metadata& gzip_image_reader::get_metadata() const noexcept
{
	return m_metadata;
}

void gzip_image_reader::read(
	array_ref destination,
	const image_transfer_plan &regions
) const
{
	// Refuses an array that is not initialized.
	auto *array_data = get_array_data(destination);
	const auto &target = destination.get_descriptor();

	// A region whose parts lie far apart, such as a slice across the planes
	// of a volume, is staged a part at a time, as a queued read reads it,
	// rather than with everything that lies between its parts.
	const auto parts = split_scattered_regions(
		regions,
		m_layout.get_strides(),
		m_layout.get_descriptor().get_data_type(),
		get_prefetch_gap_cap(),
		get_staging_byte_budget()
	);

	// Resolving the regions checks them against the file and the array.
	const image_region_read_walk walk(
		parts,
		m_layout.get_descriptor().get_extents(),
		m_layout.get_strides(),
		target.get_extents(),
		target.get_strides(),
		target.get_offset()
	);

	const auto element_count = walk.get_layout().compute_element_count();
	const auto file_offsets = walk.get_offsets().get_file();
	if (file_offsets.empty() || element_count == 0)
	{
		return;
	}

	gzip_file_stream stream(m_path);
	if (
		lie_as_they_land(walk, target.get_data_type(), m_layout) &&
		!overlap(file_offsets, element_count)
	)
	{
		read_direct(stream, m_layout, walk, array_data);
	}
	else
	{
		read_staged(
			stream,
			m_layout,
			parts,
			walk,
			array_data,
			target.get_data_type()
		);
	}
}

} // namespace vitrio
