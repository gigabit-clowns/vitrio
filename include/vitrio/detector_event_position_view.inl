// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "detector_event_position_view.hpp"

#include <stdexcept>

namespace vitrio
{

inline detector_event_position_view::detector_event_position_view(
	span<const std::uint32_t> coordinates,
	std::size_t rank
)
	: m_coordinates(coordinates)
	, m_rank(rank)
{
	if (rank == 0)
	{
		throw std::invalid_argument(
			"detector_event_position_view: The rank must not be zero."
		);
	}
	if (coordinates.size() % rank != 0)
	{
		throw std::invalid_argument(
			"detector_event_position_view: The number of coordinates is not "
			"a multiple of the rank."
		);
	}
}

inline detector_event_position_view::detector_event_position_view(
	span<const std::uint32_t> coordinates,
	std::size_t rank,
	unchecked_tag
) noexcept
	: m_coordinates(coordinates)
	, m_rank(rank)
{
}

inline std::size_t detector_event_position_view::get_rank() const noexcept
{
	return m_rank;
}

inline std::size_t
detector_event_position_view::get_event_count() const noexcept
{
	return m_coordinates.size() / m_rank;
}

inline bool detector_event_position_view::empty() const noexcept
{
	return m_coordinates.empty();
}

inline span<const std::uint32_t>
detector_event_position_view::get(std::size_t event) const noexcept
{
	return make_span(m_coordinates.data() + (event * m_rank), m_rank);
}

inline std::uint32_t detector_event_position_view::operator()(
	std::size_t event,
	std::size_t axis
) const noexcept
{
	return m_coordinates[event * m_rank + axis];
}

inline detector_event_position_view detector_event_position_view::get_events(
	std::size_t first,
	std::size_t count
) const noexcept
{
	return detector_event_position_view(
		make_span(m_coordinates.data() + (first * m_rank), count * m_rank),
		m_rank,
		unchecked_tag()
	);
}

inline span<const std::uint32_t>
detector_event_position_view::get_coordinates() const noexcept
{
	return m_coordinates;
}

} // namespace vitrio
