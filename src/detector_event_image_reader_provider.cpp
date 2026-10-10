// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/detector_event_image_reader_provider.hpp>

#include <vitrio/detector_event_image_reader.hpp>
#include <vitrio/detector_event_renderer.hpp>
#include <vitrio/detector_event_timeline_reader.hpp>
#include <vitrio/detector_event_timeline_reader_provider.hpp>

#include <stdexcept>
#include <utility>

namespace vitrio
{

detector_event_image_reader_provider::detector_event_image_reader_provider(
	std::shared_ptr<detector_event_timeline_reader_provider> events,
	detector_event_fractionation fractionation,
	std::shared_ptr<const detector_event_renderer> renderer
)
	: m_events(std::move(events))
	, m_fractionation(fractionation)
	, m_renderer(std::move(renderer))
{
	if (!m_events)
	{
		throw std::invalid_argument(
			"detector_event_image_reader_provider: The provider of the "
			"events must not be null."
		);
	}
	if (!m_renderer)
	{
		throw std::invalid_argument(
			"detector_event_image_reader_provider: The renderer must not be "
			"null."
		);
	}
}

detector_event_image_reader_provider::
~detector_event_image_reader_provider() = default;

std::shared_ptr<const image_reader>
detector_event_image_reader_provider::acquire(const std::string &key)
{
	return std::make_shared<detector_event_image_reader>(
		m_events->acquire(key),
		m_fractionation,
		m_renderer
	);
}

} // namespace vitrio
