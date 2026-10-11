// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

class detector_event_timeline_descriptor;

/**
 * @brief How many pixels of a detector an image gathers into one, along each
 * axis.
 *
 * A factor of 1 renders at the resolution of the detector, 2 merges blocks
 * of 2 by 2 pixels, and so on. It is stated in pixels of the detector rather
 * than in the quanta its events are placed with, so the same binning applies
 * to any acquisition, whatever subdivisions of a pixel it records.
 *
 * The factor must divide the pixels along every axis of an acquisition it is
 * applied to, so that every pixel of an image gathers as many of the
 * detector as any other.
 *
 * @see counting_detector_event_renderer
 */
class detector_event_binning
{
public:
	/**
	 * @brief Construct a binning by a factor.
	 *
	 * @param factor Pixels of the detector merged into one along each axis.
	 * @throws std::invalid_argument If @p factor is zero.
	 */
	VITRIO_API
	explicit detector_event_binning(std::size_t factor);

	VITRIO_API
	detector_event_binning(const detector_event_binning &other) noexcept;
	VITRIO_API
	~detector_event_binning();

	VITRIO_API
	detector_event_binning&
	operator=(const detector_event_binning &other) noexcept;

	/**
	 * @brief Get how many pixels of the detector are merged into one.
	 *
	 * @return std::size_t The factor along each axis. Never zero.
	 */
	VITRIO_API
	std::size_t get_factor() const noexcept;

	/**
	 * @brief Get how many position quanta a pixel of an image gathers.
	 *
	 * @param events The grid the events of an acquisition are placed on.
	 * @return std::vector<std::size_t> The extents of the block of quanta a
	 * pixel gathers, slowest axis first: the subdivisions of a pixel of the
	 * detector times the factor.
	 * @throws std::invalid_argument If the factor does not divide the pixels
	 * of the detector along an axis of @p events.
	 */
	VITRIO_API
	std::vector<std::size_t>
	resolve(const detector_event_timeline_descriptor &events) const;

	friend bool operator==(
		const detector_event_binning &lhs,
		const detector_event_binning &rhs
	) noexcept
	{
		return lhs.m_factor == rhs.m_factor;
	}

	friend bool operator!=(
		const detector_event_binning &lhs,
		const detector_event_binning &rhs
	) noexcept
	{
		return !(lhs == rhs);
	}

private:
	std::size_t m_factor;
};

} // namespace vitrio
