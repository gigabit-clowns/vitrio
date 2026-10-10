// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/detector_event_timeline.hpp>

#include <assert.hpp>

#include <stdexcept>

namespace vitrio
{

detector_event_timeline::detector_event_timeline(std::size_t rank)
	: m_rank(rank)
{
	if (rank == 0)
	{
		throw std::invalid_argument(
			"detector_event_timeline: The rank must not be zero."
		);
	}
}

detector_event_timeline::detector_event_timeline(
	const detector_event_timeline &other
) = default;
detector_event_timeline::detector_event_timeline(
	detector_event_timeline &&other
) noexcept = default;
detector_event_timeline::~detector_event_timeline() = default;

detector_event_timeline& detector_event_timeline::operator=(
	const detector_event_timeline &other
) = default;
detector_event_timeline& detector_event_timeline::operator=(
	detector_event_timeline &&other
) noexcept = default;

void detector_event_timeline::add_group(
	std::uint64_t timestamp,
	detector_event_position_view positions
)
{
	if (!m_timestamps.empty() && timestamp <= m_timestamps.back())
	{
		throw std::invalid_argument(
			"detector_event_timeline::add_group: The timestamp is not later "
			"than that of the last group."
		);
	}
	if (positions.get_rank() != m_rank)
	{
		throw std::invalid_argument(
			"detector_event_timeline::add_group: The positions do not have "
			"the rank of the timeline."
		);
	}

	const auto coordinates = positions.get_coordinates();
	m_group_offsets.push_back(get_event_count());
	m_timestamps.push_back(timestamp);
	m_coordinates.insert(
		m_coordinates.end(), coordinates.begin(), coordinates.end());
}

void detector_event_timeline::clear() noexcept
{
	m_timestamps.clear();
	m_group_offsets.clear();
	m_coordinates.clear();
}

void detector_event_timeline::reserve(
	std::size_t group_count,
	std::size_t event_count
)
{
	m_timestamps.reserve(group_count);
	m_group_offsets.reserve(group_count);
	m_coordinates.reserve(event_count * m_rank);
}

std::size_t detector_event_timeline::get_rank() const noexcept
{
	return m_rank;
}

std::size_t detector_event_timeline::get_group_count() const noexcept
{
	return m_timestamps.size();
}

std::size_t detector_event_timeline::get_event_count() const noexcept
{
	return m_coordinates.size() / m_rank;
}

std::uint64_t
detector_event_timeline::get_timestamp(std::size_t group) const noexcept
{
	VITRIO_ASSERT(group < m_timestamps.size());
	return m_timestamps[group];
}

detector_event_position_view
detector_event_timeline::get_positions(std::size_t group) const noexcept
{
	VITRIO_ASSERT(group < m_group_offsets.size());
	const auto first = m_group_offsets[group];
	const auto end = group + 1 < m_group_offsets.size()
		? m_group_offsets[group + 1]
		: get_event_count();

	return get_positions().get_events(first, end - first);
}

detector_event_position_view
detector_event_timeline::get_positions() const noexcept
{
	// It does not throw: the rank was checked when this timeline was made,
	// and every group adds whole rows of it.
	return detector_event_position_view(
		make_span(m_coordinates.data(), m_coordinates.size()),
		m_rank
	);
}

} // namespace vitrio
