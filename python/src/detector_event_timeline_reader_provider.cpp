// SPDX-License-Identifier: LGPL-2.1-or-later

#include "detector_event_timeline_reader_provider.hpp"

#include "utf8_string.hpp"

#include <vitrio/detector_event_timeline_reader.hpp>
#include <vitrio/detector_event_timeline_reader_provider.hpp>

#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/shared_ptr.h>

#include <filesystem>
#include <memory>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

// A reader is const in every method it has, so it is handed to Python as it
// is: Python has no const to keep it in.
std::shared_ptr<detector_event_timeline_reader> acquire(
	detector_event_timeline_reader_provider &self,
	const std::filesystem::path &key
)
{
	const auto text = to_utf8_string(key);
	const nb::gil_scoped_release release;
	return std::const_pointer_cast<detector_event_timeline_reader>(
		self.acquire(text));
}

} // anonymous namespace

void bind_detector_event_timeline_reader_provider(nb::module_ &m)
{
	nb::class_<detector_event_timeline_reader_provider>(
		m,
		"DetectorEventTimelineReaderProvider"
	)
		.def("acquire", &acquire, nb::arg("key"));
}

} // namespace vitrio
