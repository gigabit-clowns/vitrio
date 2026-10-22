// SPDX-License-Identifier: LGPL-2.1-or-later

#include "unaligned_mapped_image_reader.hpp"

#include <formats/strided_transfer/image_page_regions.hpp>
#include <formats/strided_transfer/image_region_bounds.hpp>
#include <formats/strided_transfer/image_region_read_walk.hpp>
#include <formats/strided_transfer/image_region_transfer.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/span.hpp>

#include <array/array_data.hpp>
#include <array/checked_arithmetic.hpp>

#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace vitrio
{

namespace
{

const std::size_t page_rank = 2;

[[noreturn]] void throw_short_file(const std::string &path)
{
	throw image_file_format_error(
		path + ": unaligned_mapped_image_reader: The file is shorter than "
		"the values it is said to hold."
	);
}

image_descriptor check_rank(image_descriptor descriptor)
{
	const auto rank = descriptor.get_extents().size();
	if (rank != page_rank && rank != page_rank + 1)
	{
		throw std::invalid_argument(
			"unaligned_mapped_image_reader: The values are neither an image "
			"nor a stack of them."
		);
	}

	return descriptor;
}

// Whether the values reach past the end of the mapping, worked out without
// trusting their extents not to overflow.
bool reaches_past(
	const image_file_mapping &mapping,
	const image_descriptor &descriptor,
	std::size_t data_offset
)
{
	const auto limit = static_cast<std::size_t>(
		std::numeric_limits<std::ptrdiff_t>::max()
	);

	auto size = static_cast<std::ptrdiff_t>(
		get_size(descriptor.get_data_type())
	);
	for (const auto extent : descriptor.get_extents())
	{
		if (extent > limit ||
			!checked_multiply(size, static_cast<std::ptrdiff_t>(extent), size))
		{
			return true;
		}
	}

	return data_offset > mapping.get_size() ||
		static_cast<std::size_t>(size) > mapping.get_size() - data_offset;
}

} // anonymous namespace

bool unaligned_mapped_image_reader::supports(
	const raw_image_layout &layout
) noexcept
{
	const auto rank = layout.get_descriptor().get_extents().size();
	return
		layout.is_contiguous() &&
		(rank == page_rank || rank == page_rank + 1);
}

unaligned_mapped_image_reader::unaligned_mapped_image_reader(
	const std::string &path,
	image_descriptor descriptor,
	std::size_t data_offset,
	byte_order order,
	image_metadata metadata
)
	: m_mapping(path, image_file_access::read_only)
	, m_descriptor(check_rank(std::move(descriptor)))
	, m_data_offset(data_offset)
	, m_byte_order(order)
	, m_metadata(std::move(metadata))
	, m_row_size(0)
	, m_page_size(0)
{
	if (reaches_past(m_mapping, m_descriptor, m_data_offset))
	{
		throw_short_file(path);
	}

	// Neither overflows: the values fit in the mapping.
	const auto extents = m_descriptor.get_extents();
	const auto rank = extents.size();
	m_row_size = extents[rank - 1] * get_size(m_descriptor.get_data_type());
	m_page_size = extents[rank - 2] * m_row_size;
}

const image_descriptor&
unaligned_mapped_image_reader::get_descriptor() const noexcept
{
	return m_descriptor;
}

const image_metadata&
unaligned_mapped_image_reader::get_metadata() const noexcept
{
	return m_metadata;
}

void unaligned_mapped_image_reader::read(
	array_ref destination,
	const image_transfer_plan &regions
) const
{
	auto *array_data = get_array_data(destination);

	const auto &descriptor = destination.get_descriptor();
	const auto array_extents = descriptor.get_extents();
	const auto array_strides = descriptor.get_strides();

	check_region_bounds(
		regions,
		m_descriptor,
		array_extents,
		array_strides,
		descriptor.get_offset()
	);
	if (regions.get_region_count() == 0)
	{
		return;
	}

	const image_page_regions pages(regions);

	const auto extents = m_descriptor.get_extents();
	const auto rank = extents.size();
	const std::array<std::size_t, page_rank> page_extents = {{
		extents[rank - 2],
		extents[rank - 1]
	}};
	const std::array<std::ptrdiff_t, page_rank> page_strides = {{
		static_cast<std::ptrdiff_t>(extents[rank - 1]),
		1
	}};

	// Left uninitialized: only the rows the regions reach are written, and
	// only they are read.
	const auto unit = sizeof(std::max_align_t);
	const std::unique_ptr<std::max_align_t[]> page(
		new std::max_align_t[(m_page_size + unit - 1) / unit]
	);
	auto *page_data = reinterpret_cast<byte*>(page.get());

	const auto *file_data = m_mapping.get_data() + m_data_offset;
	const auto page_count = pages.get_page_count();
	for (std::size_t position = 0; position < page_count; ++position)
	{
		const auto first_byte = pages.get_page(position) * m_page_size +
			pages.get_first_row(position) * m_row_size;
		std::memcpy(
			page_data + pages.get_first_row(position) * m_row_size,
			file_data + first_byte,
			pages.get_row_count(position) * m_row_size
		);

		const image_region_read_walk walk(
			pages.get_regions(position),
			make_span(page_extents),
			make_span(page_strides),
			array_extents,
			array_strides,
			descriptor.get_offset()
		);

		read_regions(
			walk,
			array_data,
			descriptor.get_data_type(),
			page_data,
			m_descriptor.get_data_type(),
			m_byte_order
		);
	}
}

} // namespace vitrio
