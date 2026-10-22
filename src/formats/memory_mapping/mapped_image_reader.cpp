// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mapped_image_reader.hpp"

#include "image_prefetched_read.hpp"

#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/memory_range.hpp>
#include <vitrio/span.hpp>

#include <system/page_prefetch.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace vitrio
{

namespace
{

raw_image_layout
check_aligned(raw_image_layout layout, const std::string &path)
{
	if (!mapped_image_reader::supports(layout))
	{
		throw image_file_format_error(
			path + ": mapped_image_reader: The values of the file do not "
			"begin at an offset their data type can be addressed at."
		);
	}

	return layout;
}

array_descriptor describe_values(const raw_image_layout &layout)
{
	const auto &descriptor = layout.get_descriptor();
	const auto extents = descriptor.get_extents();
	const auto strides = layout.get_strides();
	const auto data_type = descriptor.get_data_type();

	// The layout begins its values at a multiple of their size.
	const auto offset = layout.get_data_offset() / get_size(data_type);

	return array_descriptor(
		std::vector<std::size_t>(extents.begin(), extents.end()),
		std::vector<std::ptrdiff_t>(strides.begin(), strides.end()),
		static_cast<std::ptrdiff_t>(offset),
		data_type
	);
}

} // anonymous namespace

mapped_image_reader::mapped_image_reader(
	const std::string &path,
	raw_image_layout layout,
	image_metadata metadata
)
	: m_mapping(path, image_file_access::read_only)
	, m_layout(check_aligned(std::move(layout), path))
	, m_metadata(std::move(metadata))
	, m_values(describe_values(m_layout))
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

bool mapped_image_reader::supports(const raw_image_layout &layout) noexcept
{
	// A mapping addresses the values where they lie.
	return layout.is_aligned();
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
	const auto mapping = make_span(m_mapping.get_data(), m_mapping.get_size());
	const auto file = m_mapping.get_file();
	read_prefetched_regions(
		mapping,
		m_values,
		m_layout.get_byte_order(),
		destination,
		regions,
		[file, mapping](span<const memory_range> ranges)
		{
			prefetch_pages(file, mapping, ranges);
		}
	);
}

} // namespace vitrio
