// SPDX-License-Identifier: LGPL-2.1-or-later

#include "eer_detector_event_timeline_reader.hpp"

#include "eer_strip_decoder.hpp"

#include <vitrio/detector_event_position_view.hpp>
#include <vitrio/detector_event_timeline.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/span.hpp>

#include <algorithm>
#include <stdexcept>

namespace vitrio
{
namespace eer
{

namespace
{

const std::size_t rank = 2;

const std::uint64_t max_grid_extent = std::uint64_t(1) << 32;

tiff::tiff_file& select_first_frame(
	tiff::tiff_file &file,
	const std::string &path
)
{
	if (file.get_page_count() == 0)
	{
		throw image_file_format_error(
			path + ": eer: The file holds no frames."
		);
	}

	file.select_page(0);
	return file;
}

detector_event_timeline_descriptor make_descriptor(
	const std::string &path,
	const std::array<std::size_t, 2> &frame_extents,
	const eer_encoding &encoding,
	std::size_t frame_count
)
{
	const std::array<std::size_t, 2> bits = {{
		encoding.get_vertical_bits(),
		encoding.get_horizontal_bits()
	}};

	std::array<std::size_t, rank> extents;
	std::array<std::size_t, rank> subdivisions;
	for (std::size_t axis = 0; axis < rank; ++axis)
	{
		const auto extent =
			static_cast<std::uint64_t>(frame_extents[axis]) << bits[axis];
		if (extent > max_grid_extent)
		{
			throw image_file_format_error(
				path + ": eer: The frames place their events on a grid too "
				"large to address."
			);
		}
		extents[axis] = static_cast<std::size_t>(extent);
		subdivisions[axis] = std::size_t(1) << bits[axis];
	}

	return detector_event_timeline_descriptor(
		make_span(extents),
		make_span(subdivisions),
		frame_count,
		0.0
	);
}

} // anonymous namespace

eer_detector_event_timeline_reader::eer_detector_event_timeline_reader(
	const std::string &path
)
	: m_path(path)
	, m_mutex()
	, m_file(path, tiff::tiff_file_mode::read)
	, m_frame_extents(select_first_frame(m_file, m_path).get_page_extents())
	, m_encoding(read_eer_encoding(m_file, m_path))
	, m_descriptor(make_descriptor(
		m_path,
		m_frame_extents,
		m_encoding,
		m_file.get_page_count()
	))
	, m_strip()
	, m_positions()
{
}

const detector_event_timeline_descriptor&
eer_detector_event_timeline_reader::get_descriptor() const noexcept
{
	return m_descriptor;
}

void eer_detector_event_timeline_reader::read(
	std::uint64_t time_begin,
	std::uint64_t time_end,
	detector_event_timeline &destination
) const
{
	if (time_begin > time_end)
	{
		throw std::invalid_argument(
			"eer_detector_event_timeline_reader::read: The time begins after "
			"it ends."
		);
	}
	if (time_end > m_descriptor.get_time_extent())
	{
		throw std::out_of_range(
			"eer_detector_event_timeline_reader::read: The time reaches past "
			"the last frame."
		);
	}
	if (destination.get_rank() != rank)
	{
		throw std::invalid_argument(
			"eer_detector_event_timeline_reader::read: The destination does "
			"not have the rank of the events."
		);
	}

	destination.clear();

	const std::lock_guard<std::mutex> lock(m_mutex);
	for (auto frame = time_begin; frame < time_end; ++frame)
	{
		decode_frame(static_cast<std::size_t>(frame));
		if (!m_positions.empty())
		{
			destination.add_group(
				frame,
				detector_event_position_view(make_span(m_positions), rank)
			);
		}
	}
}

void eer_detector_event_timeline_reader::decode_frame(std::size_t frame) const
{
	m_file.select_page(frame);
	if (m_file.get_page_extents() != m_frame_extents ||
		read_eer_encoding(m_file, m_path) != m_encoding)
	{
		throw image_file_format_error(
			m_path + ": eer: A frame differs from the first one in size or "
			"encoding."
		);
	}

	const auto height = m_frame_extents[0];
	const auto rows_per_strip = m_file.get_rows_per_strip();
	const auto strip_count = m_file.get_strip_count();
	const eer_strip_decoder decoder(m_encoding, m_frame_extents[1]);

	m_positions.clear();
	for (std::size_t strip = 0; strip < strip_count; ++strip)
	{
		const auto first_row = strip * rows_per_strip;
		if (first_row >= height)
		{
			throw image_file_format_error(
				m_path + ": eer: A frame has more strips than rows to put in "
				"them."
			);
		}

		m_file.read_raw_strip(strip, m_strip);
		const auto decoded = decoder.decode(
			make_span(m_strip),
			first_row,
			std::min(rows_per_strip, height - first_row),
			m_positions
		);
		if (!decoded)
		{
			throw image_file_format_error(
				m_path + ": eer: A frame places an event past its last pixel, "
				"or ends within a code."
			);
		}
	}
}

} // namespace eer
} // namespace vitrio
