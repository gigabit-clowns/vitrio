// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/file_detector_event_timeline_reader_provider.hpp>

#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>
#include <vitrio/detector_event_timeline_reader.hpp>

#include <stdexcept>
#include <utility>

namespace vitrio
{

file_detector_event_timeline_reader_provider::
file_detector_event_timeline_reader_provider(
	std::shared_ptr<
		const detector_event_timeline_file_read_format_selector
	> formats
)
	: m_formats(std::move(formats))
{
	if (!m_formats)
	{
		throw std::invalid_argument(
			"file_detector_event_timeline_reader_provider: The format "
			"selector must not be null."
		);
	}
}

file_detector_event_timeline_reader_provider::
~file_detector_event_timeline_reader_provider() = default;

std::shared_ptr<const detector_event_timeline_reader>
file_detector_event_timeline_reader_provider::acquire(const std::string &key)
{
	return m_formats->open(key);
}

} // namespace vitrio
