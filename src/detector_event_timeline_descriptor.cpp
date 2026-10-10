// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/detector_event_timeline_descriptor.hpp>

#include <cmath>
#include <stdexcept>

namespace vitrio
{

namespace
{

const std::uint64_t max_extent = std::uint64_t(1) << 32;

} // anonymous namespace

detector_event_timeline_descriptor::detector_event_timeline_descriptor(
	span<const std::size_t> extents,
	span<const std::size_t> subdivisions,
	std::uint64_t time_extent,
	double time_quantum
)
	: m_extents(extents.begin(), extents.end())
	, m_subdivisions(subdivisions.begin(), subdivisions.end())
	, m_time_extent(time_extent)
	, m_time_quantum(time_quantum)
{
	if (extents.size() == 0)
	{
		throw std::invalid_argument(
			"detector_event_timeline_descriptor: The extents must not be "
			"empty."
		);
	}
	if (subdivisions.size() != extents.size())
	{
		throw std::invalid_argument(
			"detector_event_timeline_descriptor: The subdivisions must have "
			"the rank of the extents."
		);
	}

	for (std::size_t axis = 0; axis < extents.size(); ++axis)
	{
		const auto extent = extents[axis];
		const auto subdivision = subdivisions[axis];
		if (extent == 0 || subdivision == 0)
		{
			throw std::invalid_argument(
				"detector_event_timeline_descriptor: An extent or a "
				"subdivision is zero."
			);
		}
		if (static_cast<std::uint64_t>(extent) > max_extent)
		{
			throw std::invalid_argument(
				"detector_event_timeline_descriptor: An extent exceeds 2^32."
			);
		}
		if (extent % subdivision != 0)
		{
			throw std::invalid_argument(
				"detector_event_timeline_descriptor: An extent is not "
				"divided by its subdivision."
			);
		}
	}

	if (time_extent == 0)
	{
		throw std::invalid_argument(
			"detector_event_timeline_descriptor: The time extent must not "
			"be zero."
		);
	}
	if (!std::isfinite(time_quantum) || time_quantum < 0.0)
	{
		throw std::invalid_argument(
			"detector_event_timeline_descriptor: The time quantum must be "
			"finite and not negative."
		);
	}
}

detector_event_timeline_descriptor::detector_event_timeline_descriptor(
	const detector_event_timeline_descriptor &other
) = default;
detector_event_timeline_descriptor::detector_event_timeline_descriptor(
	detector_event_timeline_descriptor &&other
) noexcept = default;
detector_event_timeline_descriptor::~detector_event_timeline_descriptor() =
	default;

detector_event_timeline_descriptor&
detector_event_timeline_descriptor::operator=(
	const detector_event_timeline_descriptor &other
) = default;
detector_event_timeline_descriptor&
detector_event_timeline_descriptor::operator=(
	detector_event_timeline_descriptor &&other
) noexcept = default;

std::size_t detector_event_timeline_descriptor::get_rank() const noexcept
{
	return m_extents.size();
}

span<const std::size_t>
detector_event_timeline_descriptor::get_extents() const noexcept
{
	return make_span(m_extents.data(), m_extents.size());
}

span<const std::size_t>
detector_event_timeline_descriptor::get_subdivisions() const noexcept
{
	return make_span(m_subdivisions.data(), m_subdivisions.size());
}

std::uint64_t
detector_event_timeline_descriptor::get_time_extent() const noexcept
{
	return m_time_extent;
}

double detector_event_timeline_descriptor::get_time_quantum() const noexcept
{
	return m_time_quantum;
}

} // namespace vitrio
