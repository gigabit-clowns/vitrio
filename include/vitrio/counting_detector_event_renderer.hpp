// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_renderer.hpp>
#include <vitrio/export.hpp>
#include <vitrio/platform.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

/**
 * @brief A renderer that counts the events landing on each pixel.
 *
 * A pixel of the image gathers a bin of the grid the events are placed on:
 * a block of position quanta, of the same extents everywhere. A bin as large
 * as a pixel of the detector renders at the resolution of the detector, and
 * a smaller one renders finer than that, as far as the detector places its
 * events. The last pixel along an axis gathers what is left of the grid when
 * the bin does not divide it.
 *
 * Counts are worked out as 32 bit unsigned integers.
 */
class VITRIO_API counting_detector_event_renderer final
	: public detector_event_renderer
{
public:
	/**
	 * @brief Construct a renderer gathering bins of given extents.
	 *
	 * @param bin_extents How many position quanta a pixel of the image
	 * gathers along each axis, slowest first.
	 * @throws std::invalid_argument If @p bin_extents is empty or holds a
	 * zero.
	 */
	explicit counting_detector_event_renderer(
		span<const std::size_t> bin_extents
	);

	~counting_detector_event_renderer() override;

	/**
	 * @brief Get the extents of the bin a pixel gathers.
	 *
	 * @return span<const std::size_t> The extents in position quanta,
	 * slowest axis first. It refers to storage owned by this renderer.
	 */
	span<const std::size_t> get_bin_extents() const noexcept;

	/**
	 * @copydoc detector_event_renderer::get_image_extents
	 *
	 * Each extent is that of the grid divided by that of the bin, rounded
	 * up.
	 */
	std::vector<std::size_t> get_image_extents(
		const detector_event_timeline_descriptor &events
	) const override;

	numerical_type get_data_type() const noexcept override;

	/**
	 * @copydoc detector_event_renderer::render
	 *
	 * @throws std::overflow_error If a pixel gathers more events than 32
	 * bits count.
	 */
	void render(
		const detector_event_timeline_descriptor &descriptor,
		const detector_event_timeline &events,
		array_ref destination
	) const override;

private:
	VITRIO_STD_MEMBER_INTERFACE
	std::vector<std::size_t> m_bin_extents;
};

} // namespace vitrio
