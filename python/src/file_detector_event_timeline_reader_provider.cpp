// SPDX-License-Identifier: LGPL-2.1-or-later

#include "file_detector_event_timeline_reader_provider.hpp"

#include <vitrio/detector_event_timeline_file_read_format_selector.hpp>
#include <vitrio/detector_event_timeline_reader_provider.hpp>
#include <vitrio/file_detector_event_timeline_reader_provider.hpp>

#include <nanobind/stl/shared_ptr.h>

#include <memory>

namespace vitrio
{

namespace nb = nanobind;

void bind_file_detector_event_timeline_reader_provider(nb::module_ &m)
{
	nb::class_<
		file_detector_event_timeline_reader_provider,
		detector_event_timeline_reader_provider
	>(
		m, "FileDetectorEventTimelineReaderProvider",
		"Opens the file at a path with the format that suits it."
	)
		.def(
			nb::init<std::shared_ptr<
				const detector_event_timeline_file_read_format_selector
			>>(),
			nb::arg("formats"),
			"Make a provider.\n"
			"\n"
			"Args:\n"
			"    formats: The `DetectorEventTimelineFileReadFormatSelector` "
			"that opens the files."
		);
}

} // namespace vitrio
