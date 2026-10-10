// SPDX-License-Identifier: LGPL-2.1-or-later

#include "guarded_detector_event_timeline.hpp"

#include <nanobind/nanobind.h>

namespace vitrio
{

namespace nb = nanobind;

namespace
{

void check_not_writing(bool writing)
{
	if (writing)
	{
		throw nb::buffer_error(
			"DetectorEventTimeline: The timeline is being changed."
		);
	}
}

} // anonymous namespace

guarded_detector_event_timeline::read_guard::read_guard(
	guarded_detector_event_timeline &timeline
)
	: m_timeline(timeline)
{
	check_not_writing(timeline.m_writing);
	++timeline.m_reader_count;
}

guarded_detector_event_timeline::read_guard::~read_guard()
{
	--m_timeline.m_reader_count;
}

const detector_event_timeline&
guarded_detector_event_timeline::read_guard::get() const noexcept
{
	return m_timeline.m_timeline;
}

guarded_detector_event_timeline::write_guard::write_guard(
	guarded_detector_event_timeline &timeline
)
	: m_timeline(timeline)
{
	check_not_writing(timeline.m_writing);
	if (timeline.m_view_count != 0)
	{
		throw nb::buffer_error(
			"DetectorEventTimeline: The timeline can not be changed while "
			"a view of it is alive."
		);
	}
	if (timeline.m_reader_count != 0)
	{
		throw nb::buffer_error(
			"DetectorEventTimeline: The timeline can not be changed while "
			"it is read."
		);
	}
	timeline.m_writing = true;
}

guarded_detector_event_timeline::write_guard::~write_guard()
{
	m_timeline.m_writing = false;
}

detector_event_timeline&
guarded_detector_event_timeline::write_guard::get() const noexcept
{
	return m_timeline.m_timeline;
}

guarded_detector_event_timeline::guarded_detector_event_timeline(
	std::size_t rank
)
	: m_timeline(rank)
	, m_view_count(0)
	, m_reader_count(0)
	, m_writing(false)
{
}

void guarded_detector_event_timeline::add_view()
{
	check_not_writing(m_writing);
	++m_view_count;
}

void guarded_detector_event_timeline::remove_view() noexcept
{
	--m_view_count;
}

std::size_t guarded_detector_event_timeline::get_view_count() const noexcept
{
	return m_view_count;
}

} // namespace vitrio
