// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace vitrio
{

/**
 * @brief The events a detector counted over a stretch of time.
 *
 * Events are held in groups of one timestamp each, the groups in strictly
 * ascending order of their timestamps. A detector that reads out frames
 * gives a group per frame, its timestamp the index of the frame. One that
 * stamps each event with its own time gives a group per distinct time, so
 * that the events of an instant still share one timestamp. A time at which
 * nothing was counted needs no group.
 *
 * An event is its position: one coordinate per axis, slowest first. Times
 * and positions are counted in the quanta a
 * @ref detector_event_timeline_descriptor states, which the timeline itself
 * does not carry.
 *
 * The coordinates of every event are held one after another in one flat
 * vector, and each group is a range of it, so a timeline of any length costs
 * a bounded number of allocations. @ref clear keeps the capacity: a timeline
 * refilled after clearing allocates nothing until it outgrows what it held
 * before.
 *
 * The rank is stated when a timeline is constructed and never changes.
 */
class detector_event_timeline
{
public:
	/**
	 * @brief Construct an empty timeline of a given rank.
	 *
	 * @param rank Number of coordinates each event carries.
	 * @throws std::invalid_argument If @p rank is zero.
	 */
	VITRIO_API
	explicit detector_event_timeline(std::size_t rank);

	VITRIO_API
	detector_event_timeline(const detector_event_timeline &other);
	VITRIO_API
	detector_event_timeline(detector_event_timeline &&other) noexcept;
	VITRIO_API
	~detector_event_timeline();

	VITRIO_API
	detector_event_timeline& operator=(const detector_event_timeline &other);
	VITRIO_API
	detector_event_timeline&
	operator=(detector_event_timeline &&other) noexcept;

	/**
	 * @brief Append the events of one timestamp.
	 *
	 * @param timestamp Time of the events. Must be later than that of the
	 * group appended last.
	 * @param coordinates The coordinates of the events, those of one event
	 * after another. Their number must be a multiple of @ref get_rank.
	 * @throws std::invalid_argument If @p timestamp is not later than that
	 * of the last group, or if the number of @p coordinates is not a
	 * multiple of the rank.
	 */
	VITRIO_API
	void add_group(
		std::uint64_t timestamp,
		span<const std::uint32_t> coordinates
	);

	/**
	 * @brief Drop the events held, keeping the rank and the capacity.
	 */
	VITRIO_API
	void clear() noexcept;

	/**
	 * @brief Make room for a number of groups and events without allocating
	 * later.
	 *
	 * @param group_count Number of groups to make room for.
	 * @param event_count Number of events to make room for.
	 */
	VITRIO_API
	void reserve(std::size_t group_count, std::size_t event_count);

	/**
	 * @brief Get the number of coordinates each event carries.
	 *
	 * @return std::size_t The rank. Never zero.
	 */
	VITRIO_API
	std::size_t get_rank() const noexcept;

	/**
	 * @brief Get how many groups are held.
	 *
	 * @return std::size_t The number of groups.
	 */
	VITRIO_API
	std::size_t get_group_count() const noexcept;

	/**
	 * @brief Get how many events are held, in every group.
	 *
	 * @return std::size_t The number of events.
	 */
	VITRIO_API
	std::size_t get_event_count() const noexcept;

	/**
	 * @brief Get the timestamp of one group.
	 *
	 * @param group Position of the group. Must be below
	 * @ref get_group_count.
	 * @return std::uint64_t The timestamp, which ascends with @p group.
	 */
	VITRIO_API
	std::uint64_t get_timestamp(std::size_t group) const noexcept;

	/**
	 * @brief Get the coordinates of the events of one group.
	 *
	 * @param group Position of the group. Must be below
	 * @ref get_group_count.
	 * @return span<const std::uint32_t> The coordinates, those of one event
	 * after another, @ref get_rank of them each. It refers to storage owned
	 * by this timeline, which adding to, assigning to or destroying it
	 * invalidates.
	 */
	VITRIO_API
	span<const std::uint32_t> get_coordinates(std::size_t group) const noexcept;

	/**
	 * @brief Get the coordinates of every event held.
	 *
	 * @return span<const std::uint32_t> The coordinates, those of one event
	 * after another and group after group, @ref get_rank of them each. It
	 * refers to storage owned by this timeline, which adding to, assigning
	 * to or destroying it invalidates.
	 */
	VITRIO_API
	span<const std::uint32_t> get_coordinates() const noexcept;

private:
	std::vector<std::uint64_t> m_timestamps;
	std::vector<std::size_t> m_group_offsets;
	std::vector<std::uint32_t> m_coordinates;
	std::size_t m_rank;
};

} // namespace vitrio
