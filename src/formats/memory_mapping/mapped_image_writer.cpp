// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mapped_image_writer.hpp"

#include <formats/strided_transfer/image_region_transfer.hpp>
#include <formats/strided_transfer/image_region_write_walk.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/const_array_ref.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <array/array_data.hpp>
#include <logger.hpp>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <utility>

namespace vitrio
{

namespace
{

image_file_mapping lay_out_file(
	const std::string &path,
	const image_file_layout &layout,
	span<const byte> preamble
)
{
	if (preamble.size() > layout.get_data_offset())
	{
		throw std::invalid_argument(
			"mapped_image_writer: The preamble reaches past where the values "
			"of the file begin."
		);
	}

	create_image_file(
		path,
		layout.get_data_offset() + layout.get_data_size()
	);

	image_file_mapping mapping(path, image_file_access::read_write);
	std::copy(preamble.begin(), preamble.end(), mapping.get_data());

	return mapping;
}

} // anonymous namespace

mapped_image_writer::mapped_image_writer(
	const std::string &path,
	image_file_layout layout,
	span<const byte> preamble
)
	: m_layout(std::move(layout))
	, m_mapping(lay_out_file(path, m_layout, preamble))
{
}

mapped_image_writer::~mapped_image_writer()
{
	try
	{
		m_mapping.flush();
	}
	catch (const std::exception &error)
	{
		VITRIO_LOG_ERROR(
			"Failed to flush an image file while closing it: {}",
			error.what()
		);
	}
	catch (...)
	{
		VITRIO_LOG_ERROR("Failed to flush an image file while closing it.");
	}
}

const image_descriptor& mapped_image_writer::get_descriptor() const noexcept
{
	return m_layout.get_descriptor();
}

void mapped_image_writer::write(
	const_array_ref source,
	const image_transfer_plan &regions
)
{
	const auto *array_data = get_array_data(source);

	const auto &descriptor = source.get_descriptor();
	const image_region_write_walk walk(
		regions,
		m_layout.get_descriptor().get_extents(),
		m_layout.get_strides(),
		descriptor.get_extents(),
		descriptor.get_strides(),
		descriptor.get_offset()
	);

	write_regions(
		walk,
		array_data,
		descriptor.get_data_type(),
		m_mapping.get_data() + m_layout.get_data_offset(),
		m_layout.get_descriptor().get_data_type(),
		m_layout.get_byte_order()
	);
}

void mapped_image_writer::flush()
{
	m_mapping.flush();
}

} // namespace vitrio
