// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/detector_event_timeline_file_read_format_registry.hpp>

#include <vitrio/detector_event_timeline_file_read_format.hpp>
#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>

namespace vitrio
{

detector_event_timeline_file_read_format_registry::
detector_event_timeline_file_read_format_registry() = default;

detector_event_timeline_file_read_format_registry::
~detector_event_timeline_file_read_format_registry() = default;

void detector_event_timeline_file_read_format_registry::add(
	detector_event_timeline_file_read_format_factory factory
)
{
	if (factory)
	{
		m_factories.push_back(factory);
	}
}

void detector_event_timeline_file_read_format_registry::register_all(
	detector_event_timeline_file_read_format_selector &selector
) const
{
	for (const auto factory : m_factories)
	{
		selector.register_format(factory());
	}
}

} // namespace vitrio
