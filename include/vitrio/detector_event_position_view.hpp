// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>

namespace vitrio
{

/**
 * @brief A view of the positions of a sequence of events, as a matrix of one
 * row per event and one column per axis.
 *
 * The coordinates are held row after row, those of one event after those of
 * the one before, slowest axis first within a row. Each is counted in the
 * position quanta of a @ref detector_event_timeline_descriptor.
 *
 * A view is a pointer, a size and a rank, as cheap to copy as a span, and
 * what it views must outlive it. It never owns or changes the coordinates.
 *
 * @see detector_event_timeline
 */
class detector_event_position_view
{
public:
	/**
	 * @brief Construct a view of coordinates held row after row.
	 *
	 * @param coordinates The coordinates of every event, those of one event
	 * after another.
	 * @param rank Number of coordinates each event carries.
	 * @throws std::invalid_argument If @p rank is zero, or if the number of
	 * @p coordinates is not a multiple of it.
	 */
	detector_event_position_view(
		span<const std::uint32_t> coordinates,
		std::size_t rank
	);

	/**
	 * @brief Get the number of coordinates each event carries.
	 *
	 * @return std::size_t The rank, the number of columns. Never zero.
	 */
	std::size_t get_rank() const noexcept;

	/**
	 * @brief Get the number of events viewed.
	 *
	 * @return std::size_t The number of rows.
	 */
	std::size_t get_event_count() const noexcept;

	/**
	 * @brief Check whether no event is viewed.
	 *
	 * @return true There are no rows.
	 * @return false There is at least one.
	 */
	bool empty() const noexcept;

	/**
	 * @brief Get the position of one event.
	 *
	 * @param event Index of the event. Must be below @ref get_event_count.
	 * @return span<const std::uint32_t> Its coordinates, @ref get_rank of
	 * them, slowest axis first.
	 */
	span<const std::uint32_t> get(std::size_t event) const noexcept;

	/**
	 * @brief Get one coordinate of one event.
	 *
	 * @param event Index of the event. Must be below @ref get_event_count.
	 * @param axis Index of the axis. Must be below @ref get_rank.
	 * @return std::uint32_t The coordinate.
	 */
	std::uint32_t operator()(std::size_t event, std::size_t axis)
	const noexcept;

	/**
	 * @brief Get the positions of a run of the events viewed.
	 *
	 * @param first Index of the first event of the run.
	 * @param count Number of events from there. The run must not reach past
	 * @ref get_event_count.
	 * @return detector_event_position_view A view of the run, of the same
	 * rank.
	 */
	detector_event_position_view
	get_events(std::size_t first, std::size_t count) const noexcept;

	/**
	 * @brief Get every coordinate viewed, row after row.
	 *
	 * For walking every event at once, where indexing them one by one
	 * would cost more than it says.
	 *
	 * @return span<const std::uint32_t> @ref get_rank coordinates per
	 * event, @ref get_event_count events.
	 */
	span<const std::uint32_t> get_coordinates() const noexcept;

private:
	struct unchecked_tag {};

	detector_event_position_view(
		span<const std::uint32_t> coordinates,
		std::size_t rank,
		unchecked_tag
	) noexcept;

	span<const std::uint32_t> m_coordinates;
	std::size_t m_rank;
};

} // namespace vitrio

#include "detector_event_position_view.inl"
