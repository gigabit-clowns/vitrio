// SPDX-License-Identifier: LGPL-2.1-or-later

#include "builtin_detector_event_timeline_file_read_format_registry.hpp"

namespace vitrio
{

detector_event_timeline_file_read_format_registry&
get_builtin_detector_event_timeline_file_read_format_registry() noexcept
{
	static detector_event_timeline_file_read_format_registry registry;
	return registry;
}

} // namespace vitrio
