// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tiff_reader.hpp"

#include <formats/strided_transfer/image_page_regions.hpp>
#include <formats/strided_transfer/image_region_bounds.hpp>
#include <formats/strided_transfer/image_region_read_walk.hpp>
#include <formats/strided_transfer/image_region_transfer.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/span.hpp>

#include <array/array_data.hpp>
#include <memory/byte_order.hpp>

#include <array>
#include <cstddef>
#include <vector>

namespace vitrio
{
namespace tiff
{

namespace
{

const std::size_t core_rank = 2;

image_descriptor make_descriptor(const tiff_page_decoder &decoder)
{
	std::vector<std::size_t> extents;
	if (decoder.get_page_count() > 1)
	{
		extents.push_back(decoder.get_page_count());
	}
	extents.push_back(decoder.get_height());
	extents.push_back(decoder.get_width());

	return image_descriptor(
		make_span(extents.data(), extents.size()),
		core_rank,
		decoder.get_data_type()
	);
}

} // anonymous namespace

tiff_reader::tiff_reader(const std::string &path)
	: m_mutex()
	, m_decoder(path)
	, m_descriptor(make_descriptor(m_decoder))
	, m_metadata()
{
}

const image_descriptor& tiff_reader::get_descriptor() const noexcept
{
	return m_descriptor;
}

const image_metadata& tiff_reader::get_metadata() const noexcept
{
	return m_metadata;
}

void tiff_reader::read(
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

	// Taken before the decoder is asked anything: decoding a page sets its
	// layout again, so even its extents are not read beside another call.
	const std::lock_guard<std::mutex> lock(m_mutex);

	const std::array<std::size_t, 2> page_extents = {{
		m_decoder.get_height(),
		m_decoder.get_width()
	}};
	const std::array<std::ptrdiff_t, 2> page_strides = {{
		static_cast<std::ptrdiff_t>(m_decoder.get_width()),
		1
	}};

	const auto page_count = pages.get_page_count();
	for (std::size_t position = 0; position < page_count; ++position)
	{
		const auto *samples = m_decoder.decode(
			pages.get_page(position),
			pages.get_first_row(position),
			pages.get_row_count(position)
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
			samples,
			m_descriptor.get_data_type(),
			get_system_byte_order()
		);
	}
}

} // namespace tiff
} // namespace vitrio
