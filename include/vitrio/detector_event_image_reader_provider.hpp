// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_fractionation.hpp>
#include <vitrio/export.hpp>
#include <vitrio/image_reader_provider.hpp>
#include <vitrio/platform.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class detector_event_renderer;
class detector_event_timeline_reader_provider;

/**
 * @brief A provider that reads acquisitions of detector events as movies,
 * each fraction of an acquisition rendered into one image.
 *
 * Every acquisition it serves is cut by one fractionation and rendered by
 * one renderer, which are what the provider is constructed with: how an
 * acquisition becomes images is chosen by whoever reads it, not by the
 * file it is in, so it is stated where readers are made rather than
 * registered with a format.
 *
 * A key means whatever it means to the provider of the acquisitions, which
 * every key is handed to. It keeps nothing: every reader it returns is newly
 * made, which @ref caching_image_reader_provider can be put in front of.
 *
 * @see detector_event_image_reader
 */
class VITRIO_API detector_event_image_reader_provider final
	: public image_reader_provider
{
public:
	/**
	 * @brief Construct a provider rendering acquisitions as given.
	 *
	 * @param events Where the acquisitions come from.
	 * @param fractionation How the time of an acquisition is cut into
	 * fractions.
	 * @param renderer How a fraction is turned into an image.
	 * @throws std::invalid_argument If @p events or @p renderer is null.
	 */
	detector_event_image_reader_provider(
		std::shared_ptr<detector_event_timeline_reader_provider> events,
		detector_event_fractionation fractionation,
		std::shared_ptr<const detector_event_renderer> renderer
	);

	~detector_event_image_reader_provider() override;

	/**
	 * @copydoc image_reader_provider::acquire
	 *
	 * @throws std::invalid_argument If the acquisition has fewer time
	 * quanta than there are fractions, or if the renderer does not render
	 * its events into images of rank two.
	 */
	std::shared_ptr<const image_reader>
	acquire(const std::string &key) override;

private:
	VITRIO_STD_MEMBER_INTERFACE
	std::shared_ptr<detector_event_timeline_reader_provider> m_events;
	detector_event_fractionation m_fractionation;
	VITRIO_STD_MEMBER_INTERFACE
	std::shared_ptr<const detector_event_renderer> m_renderer;
};

} // namespace vitrio
