// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_fractionation.hpp>
#include <vitrio/export.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/platform.hpp>

#include <memory>

namespace vitrio
{

class detector_event_renderer;
class detector_event_timeline_reader;

/**
 * @brief Reads an acquisition of detector events as a movie of the images
 * its fractions render into.
 *
 * The time of the acquisition is cut by a @ref detector_event_fractionation,
 * and each fraction is rendered into one image by a
 * @ref detector_event_renderer. A fractionation of several fractions is read
 * as a stack of images, its first axis running along the fractions, and one
 * of a single fraction as one image of every event.
 *
 * A fraction is read and rendered as a whole when a region reaches it, and
 * the fraction rendered last is kept, so regions of one fraction read by
 * successive calls render it once. Rendering goes through one image, so
 * concurrent calls to @ref read are serialised.
 *
 * Images of rank two are rendered, as a detector gives.
 */
class VITRIO_API detector_event_image_reader final
	: public image_reader
{
public:
	/**
	 * @brief Construct a reader rendering the fractions of an acquisition.
	 *
	 * @param events The acquisition.
	 * @param fractionation How its time is cut into fractions.
	 * @param renderer How a fraction is turned into an image.
	 * @throws std::invalid_argument If @p events or @p renderer is null, if
	 * there are more fractions than time quanta to cut, or if
	 * @p renderer does not render the events of @p events into images of
	 * rank two.
	 */
	detector_event_image_reader(
		std::shared_ptr<const detector_event_timeline_reader> events,
		detector_event_fractionation fractionation,
		std::shared_ptr<const detector_event_renderer> renderer
	);

	~detector_event_image_reader() override;

	/**
	 * @brief Get the fractionation the acquisition is read with.
	 *
	 * @return const detector_event_fractionation& The fractionation.
	 */
	const detector_event_fractionation& get_fractionation() const noexcept;

	const image_descriptor& get_descriptor() const noexcept override;

	const image_metadata& get_metadata() const noexcept override;

	/**
	 * @copydoc image_reader::read
	 *
	 * @throws std::out_of_range If an event lies outside the grid of the
	 * acquisition.
	 */
	void read(
		array_ref destination,
		const image_transfer_plan &regions
	) const override;

private:
	class implementation;
	VITRIO_STD_MEMBER_INTERFACE
	std::unique_ptr<implementation> m_implementation;
};

} // namespace vitrio
