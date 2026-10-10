// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_binning.hpp>
#include <vitrio/detector_event_renderer.hpp>
#include <vitrio/export.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

/**
 * @brief A renderer that counts the events landing on each pixel.
 *
 * A pixel of the image gathers a block of pixels of the detector, as many
 * as its @ref detector_event_binning says along each axis, and counts every
 * event that fell on any of them, wherever within the pixel it was placed.
 *
 * Counts are worked out as 32 bit unsigned integers.
 */
class VITRIO_API counting_detector_event_renderer final
	: public detector_event_renderer
{
public:
	/**
	 * @brief Construct a renderer gathering the pixels a binning merges.
	 *
	 * @param binning How many pixels of the detector a pixel of the image
	 * gathers along each axis.
	 */
	explicit counting_detector_event_renderer(
		detector_event_binning binning
	) noexcept;

	~counting_detector_event_renderer() override;

	/**
	 * @brief Get how many pixels of the detector a pixel gathers.
	 *
	 * @return const detector_event_binning& The binning.
	 */
	const detector_event_binning& get_binning() const noexcept;

	/**
	 * @copydoc detector_event_renderer::get_image_extents
	 *
	 * Each extent is that of the detector, in pixels, divided by the factor
	 * of the binning.
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
	detector_event_binning m_binning;
};

} // namespace vitrio
