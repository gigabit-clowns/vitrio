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
 * @brief What is fixed for one acquisition of a detector that counts
 * events: the grid its events are placed on and the time they span.
 *
 * Positions and times are integers, counted in quanta. A position quantum
 * is a fraction of a pixel of the detector, the same along every pixel of an
 * axis, so that an event is placed on a grid finer than the pixels by a whole
 * number of subdivisions per pixel. A detector that places its events no
 * finer than its pixels has one subdivision along every axis.
 *
 * Time runs from zero up to the time extent, which bounds the acquisition
 * whether or not an event falls at its end. A detector that reads out
 * frames, such as one writing EER files, counts time in frames.
 *
 * @see detector_event_timeline
 */
class detector_event_timeline_descriptor
{
public:
	/**
	 * @brief Construct a descriptor from its components.
	 *
	 * @param extents Extents of the grid of positions, in position quanta
	 * and slowest axis first. Each must be at most 2^32, so that a position
	 * along it fits in 32 bits.
	 * @param subdivisions Position quanta per pixel of the detector along
	 * each axis. Each must divide the extent of its axis.
	 * @param time_extent How many time quanta the acquisition spans.
	 * @param time_quantum Length of one time quantum in seconds, or zero
	 * when it is not known.
	 * @throws std::invalid_argument If @p extents is empty, if
	 * @p subdivisions does not have its rank, if an extent or a subdivision
	 * is zero, if an extent exceeds 2^32 or is not divided by its
	 * subdivision, if @p time_extent is zero, or if @p time_quantum is
	 * negative or not finite.
	 */
	VITRIO_API
	detector_event_timeline_descriptor(
		span<const std::size_t> extents,
		span<const std::size_t> subdivisions,
		std::uint64_t time_extent,
		double time_quantum
	);

	VITRIO_API
	detector_event_timeline_descriptor(
		const detector_event_timeline_descriptor &other
	);
	VITRIO_API
	detector_event_timeline_descriptor(
		detector_event_timeline_descriptor &&other
	) noexcept;
	VITRIO_API
	~detector_event_timeline_descriptor();

	VITRIO_API
	detector_event_timeline_descriptor&
	operator=(const detector_event_timeline_descriptor &other);
	VITRIO_API
	detector_event_timeline_descriptor&
	operator=(detector_event_timeline_descriptor &&other) noexcept;

	/**
	 * @brief Get the number of axes events are placed along.
	 *
	 * @return std::size_t The rank. Never zero.
	 */
	VITRIO_API
	std::size_t get_rank() const noexcept;

	/**
	 * @brief Get the extents of the grid of positions.
	 *
	 * @return span<const std::size_t> The extents in position quanta,
	 * slowest axis first. It refers to storage owned by this descriptor.
	 */
	VITRIO_API
	span<const std::size_t> get_extents() const noexcept;

	/**
	 * @brief Get how many position quanta a pixel of the detector spans.
	 *
	 * @return span<const std::size_t> The subdivisions of a pixel along
	 * each axis, slowest first. It refers to storage owned by this
	 * descriptor.
	 */
	VITRIO_API
	span<const std::size_t> get_subdivisions() const noexcept;

	/**
	 * @brief Get how many time quanta the acquisition spans.
	 *
	 * @return std::uint64_t The time extent. Never zero.
	 */
	VITRIO_API
	std::uint64_t get_time_extent() const noexcept;

	/**
	 * @brief Get the length of one time quantum.
	 *
	 * @return double The length in seconds, or zero when it is not known.
	 */
	VITRIO_API
	double get_time_quantum() const noexcept;

	friend bool operator==(
		const detector_event_timeline_descriptor &lhs,
		const detector_event_timeline_descriptor &rhs
	) noexcept
	{
		return
			lhs.m_time_extent == rhs.m_time_extent &&
			lhs.m_time_quantum == rhs.m_time_quantum &&
			lhs.m_extents == rhs.m_extents &&
			lhs.m_subdivisions == rhs.m_subdivisions;
	}

	friend bool operator!=(
		const detector_event_timeline_descriptor &lhs,
		const detector_event_timeline_descriptor &rhs
	) noexcept
	{
		return !(lhs == rhs);
	}

private:
	std::vector<std::size_t> m_extents;
	std::vector<std::size_t> m_subdivisions;
	std::uint64_t m_time_extent;
	double m_time_quantum;
};

} // namespace vitrio
