// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <cstdint>

namespace vitrio
{

class detector_event_timeline;
class detector_event_timeline_descriptor;

/**
 * @brief Abstract read-only view of one acquisition of a detector that counts
 * events.
 *
 * A reader exposes the events of an acquisition a stretch of time at a time,
 * so that what is read of a long acquisition is only what is asked for,
 * rather than every event it holds. How the events are stored, and whether
 * a stretch of time is decoded to be read, is the business of the reader.
 *
 * Everything a reader reports is fixed when it is opened, and reading does
 * not change it.
 *
 * @see detector_event_timeline_descriptor
 */
class VITRIO_API detector_event_timeline_reader
{
public:
	detector_event_timeline_reader() noexcept;
	detector_event_timeline_reader(
		const detector_event_timeline_reader &other
	) = delete;
	detector_event_timeline_reader(
		detector_event_timeline_reader &&other
	) = delete;
	virtual ~detector_event_timeline_reader();

	detector_event_timeline_reader&
	operator=(const detector_event_timeline_reader &other) = delete;
	detector_event_timeline_reader&
	operator=(detector_event_timeline_reader &&other) = delete;

	/**
	 * @brief Get the grid the events are placed on and the time they span.
	 *
	 * @return const detector_event_timeline_descriptor& The descriptor. It
	 * refers to storage owned by this reader.
	 */
	virtual const detector_event_timeline_descriptor&
	get_descriptor() const noexcept = 0;

	/**
	 * @brief Read the events of a stretch of time.
	 *
	 * @p destination is cleared and given the events whose timestamps lie
	 * in @c [time_begin,time_end), in groups of ascending timestamps as
	 * @ref detector_event_timeline holds them. Every coordinate lies within
	 * the extents of @ref get_descriptor.
	 *
	 * @par Thread safety
	 * This method may be called concurrently on one reader. A reader that
	 * can not decode in parallel serialises the calls itself.
	 *
	 * @param time_begin The first time quantum to read.
	 * @param time_end One past the last time quantum to read. A stretch of
	 * no time reads nothing and succeeds.
	 * @param destination Where the events are written. Must have the rank
	 * of @ref get_descriptor.
	 * @throws std::invalid_argument If @p time_begin is after @p time_end,
	 * or if @p destination does not have the rank of the events.
	 * @throws std::out_of_range If @p time_end is past the time extent of
	 * @ref get_descriptor.
	 * @throws image_file_format_error If the acquisition turns out to be
	 * malformed or truncated where it is read.
	 */
	virtual void read(
		std::uint64_t time_begin,
		std::uint64_t time_end,
		detector_event_timeline &destination
	) const = 0;
};

} // namespace vitrio
