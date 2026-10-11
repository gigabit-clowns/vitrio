// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/detector_event_binning.hpp>

#include <vitrio/detector_event_timeline_descriptor.hpp>

#include <stdexcept>

namespace vitrio
{

detector_event_binning::detector_event_binning(std::size_t factor)
	: m_factor(factor)
{
	if (factor == 0)
	{
		throw std::invalid_argument(
			"detector_event_binning: The factor must not be zero."
		);
	}
}

detector_event_binning::detector_event_binning(
	const detector_event_binning &other
) noexcept = default;
detector_event_binning::~detector_event_binning() = default;

detector_event_binning& detector_event_binning::operator=(
	const detector_event_binning &other
) noexcept = default;

std::size_t detector_event_binning::get_factor() const noexcept
{
	return m_factor;
}

std::vector<std::size_t> detector_event_binning::resolve(
	const detector_event_timeline_descriptor &events
) const
{
	const auto extents = events.get_extents();
	const auto subdivisions = events.get_subdivisions();

	std::vector<std::size_t> bin(extents.size());
	for (std::size_t axis = 0; axis < bin.size(); ++axis)
	{
		// The subdivisions divide the extents, which the descriptor checks,
		// and the factor divides what that leaves, so the bin is no larger
		// than the grid and does not overflow.
		const auto pixels = extents[axis] / subdivisions[axis];
		if (pixels % m_factor != 0)
		{
			throw std::invalid_argument(
				"detector_event_binning::resolve: The factor does not divide "
				"the pixels of the detector."
			);
		}
		bin[axis] = subdivisions[axis] * m_factor;
	}

	return bin;
}

} // namespace vitrio
