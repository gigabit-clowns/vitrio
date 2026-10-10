// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/detector_event_fractionation.hpp>

#include <assert.hpp>

#include <stdexcept>

namespace vitrio
{

constexpr std::size_t detector_event_fractionation::max_fraction_count;

detector_event_fractionation::detector_event_fractionation(
	std::size_t fraction_count
)
	: m_fraction_count(fraction_count)
{
	if (fraction_count == 0)
	{
		throw std::invalid_argument(
			"detector_event_fractionation: The fraction count must not be "
			"zero."
		);
	}
	if (fraction_count > max_fraction_count)
	{
		throw std::invalid_argument(
			"detector_event_fractionation: The fraction count exceeds the "
			"most there may be."
		);
	}
}

detector_event_fractionation::detector_event_fractionation(
	const detector_event_fractionation &other
) noexcept = default;
detector_event_fractionation::~detector_event_fractionation() = default;

detector_event_fractionation& detector_event_fractionation::operator=(
	const detector_event_fractionation &other
) noexcept = default;

std::size_t detector_event_fractionation::get_fraction_count() const noexcept
{
	return m_fraction_count;
}

std::uint64_t detector_event_fractionation::get_fraction_begin(
	std::size_t fraction,
	std::uint64_t time_extent
) const noexcept
{
	VITRIO_ASSERT(fraction <= m_fraction_count);

	// floor(i*T/N) is i*q + floor(i*r/N), with T = q*N + r. Neither term
	// overflows: i*q does not exceed T, and i and r are below 2^32 for
	// there are no more fractions than that.
	const auto count = static_cast<std::uint64_t>(m_fraction_count);
	const auto index = static_cast<std::uint64_t>(fraction);
	const auto quotient = time_extent / count;
	const auto remainder = time_extent % count;
	return index * quotient + (index * remainder) / count;
}

} // namespace vitrio
