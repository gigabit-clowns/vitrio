// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <cstddef>
#include <cstdint>

namespace vitrio
{

/**
 * @brief How the time of an acquisition is cut into fractions.
 *
 * A fixed number of fractions, of time spans as even as integers allow:
 * fraction @c i of @c N, over a time extent @c T, spans
 * @c [floor(i*T/N),floor((i+1)*T/N)). The fractions differ by one time
 * quantum at most, and the longer ones are spread over the acquisition
 * rather than gathered at either end of it. Over the frames of a movie, that
 * means some fractions hold one frame more than others.
 *
 * Where the fractions fall is the business of whoever reads the acquisition,
 * not of the acquisition, so it is stated apart from it and applied to any
 * time extent.
 */
class detector_event_fractionation
{
public:
	/**
	 * @brief The most fractions a fractionation may cut into.
	 */
	VITRIO_API
	static constexpr std::size_t max_fraction_count = 0xFFFFFFFF;

	/**
	 * @brief Construct a fractionation into a number of fractions.
	 *
	 * @param fraction_count How many fractions the time is cut into.
	 * @throws std::invalid_argument If @p fraction_count is zero or exceeds
	 * @ref max_fraction_count.
	 */
	VITRIO_API
	explicit detector_event_fractionation(std::size_t fraction_count);

	VITRIO_API
	detector_event_fractionation(
		const detector_event_fractionation &other
	) noexcept;
	VITRIO_API
	~detector_event_fractionation();

	VITRIO_API
	detector_event_fractionation&
	operator=(const detector_event_fractionation &other) noexcept;

	/**
	 * @brief Get how many fractions the time is cut into.
	 *
	 * @return std::size_t The number of fractions. Never zero.
	 */
	VITRIO_API
	std::size_t get_fraction_count() const noexcept;

	/**
	 * @brief Get the time a fraction begins at.
	 *
	 * Fraction @c i spans from where it begins up to where fraction
	 * @c i+1 does, so the fraction past the last one begins where the time
	 * ends.
	 *
	 * @param fraction Index of the fraction. Must not exceed
	 * @ref get_fraction_count.
	 * @param time_extent How many time quanta are cut.
	 * @return std::uint64_t The first time quantum of the fraction, or
	 * @p time_extent for the fraction past the last one.
	 */
	VITRIO_API
	std::uint64_t get_fraction_begin(
		std::size_t fraction,
		std::uint64_t time_extent
	) const noexcept;

	friend bool operator==(
		const detector_event_fractionation &lhs,
		const detector_event_fractionation &rhs
	) noexcept
	{
		return lhs.m_fraction_count == rhs.m_fraction_count;
	}

	friend bool operator!=(
		const detector_event_fractionation &lhs,
		const detector_event_fractionation &rhs
	) noexcept
	{
		return !(lhs == rhs);
	}

private:
	std::size_t m_fraction_count;
};

} // namespace vitrio
