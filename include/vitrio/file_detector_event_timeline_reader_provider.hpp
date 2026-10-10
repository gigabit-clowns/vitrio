// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_timeline_reader_provider.hpp>
#include <vitrio/export.hpp>
#include <vitrio/platform.hpp>

#include <memory>
#include <string>

namespace vitrio
{

class detector_event_timeline_file_read_format_selector;

/**
 * @brief A provider that reads files of detector events. It opens each file
 * through a format selector, every time it is asked for one.
 *
 * It keeps nothing: every reader it returns is newly opened through the
 * format selector it was constructed with, so a key asked for twice is
 * opened twice and the two readers are unrelated.
 *
 * A key is handed to the selector as the path of the file.
 *
 * @see file_image_reader_provider
 * @see detector_event_timeline_file_read_format_selector
 */
class VITRIO_API file_detector_event_timeline_reader_provider final
	: public detector_event_timeline_reader_provider
{
public:
	/**
	 * @brief Construct a provider opening through a format selector.
	 *
	 * @param formats The formats a file may be opened with.
	 * @throws std::invalid_argument If @p formats is null.
	 */
	explicit file_detector_event_timeline_reader_provider(
		std::shared_ptr<
			const detector_event_timeline_file_read_format_selector
		> formats
	);

	~file_detector_event_timeline_reader_provider() override;

	std::shared_ptr<const detector_event_timeline_reader>
	acquire(const std::string &key) override;

private:
	VITRIO_STD_MEMBER_INTERFACE
	std::shared_ptr<
		const detector_event_timeline_file_read_format_selector
	> m_formats;
};

} // namespace vitrio
