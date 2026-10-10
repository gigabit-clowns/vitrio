// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/detector_event_timeline.hpp>

#include <cstddef>

namespace vitrio
{

/**
 * @brief A timeline given to Python, which keeps track of what uses its
 * memory.
 *
 * Python is handed views of the memory of a timeline, which a change to the
 * timeline would leave pointing at memory it no longer holds. So the
 * timeline is not changed while a view of it is alive, as a `bytearray` is
 * not resized while a `memoryview` of it is: the change raises `BufferError`
 * instead. Nor is it changed while it is read without the GIL, nor read or
 * viewed while it is changed without the GIL.
 *
 * What it counts is only ever counted by a thread that holds the GIL, and
 * the guards are made and destroyed with it held.
 */
class guarded_detector_event_timeline
{
public:
	/**
	 * @brief Lets the timeline be read until it is destroyed.
	 */
	class read_guard
	{
	public:
		/**
		 * @brief Start reading a timeline.
		 *
		 * @param timeline The timeline.
		 * @throws nanobind::buffer_error If the timeline is being changed.
		 */
		explicit read_guard(guarded_detector_event_timeline &timeline);

		read_guard(const read_guard &other) = delete;
		read_guard(read_guard &&other) = delete;
		~read_guard();

		read_guard& operator=(const read_guard &other) = delete;
		read_guard& operator=(read_guard &&other) = delete;

		/**
		 * @brief Get the timeline being read.
		 *
		 * @return const detector_event_timeline& The timeline.
		 */
		const detector_event_timeline& get() const noexcept;

	private:
		guarded_detector_event_timeline &m_timeline;
	};

	/**
	 * @brief Lets the timeline be changed until it is destroyed.
	 */
	class write_guard
	{
	public:
		/**
		 * @brief Start changing a timeline.
		 *
		 * @param timeline The timeline.
		 * @throws nanobind::buffer_error If the timeline is viewed, read or
		 * changed meanwhile.
		 */
		explicit write_guard(guarded_detector_event_timeline &timeline);

		write_guard(const write_guard &other) = delete;
		write_guard(write_guard &&other) = delete;
		~write_guard();

		write_guard& operator=(const write_guard &other) = delete;
		write_guard& operator=(write_guard &&other) = delete;

		/**
		 * @brief Get the timeline being changed.
		 *
		 * @return detector_event_timeline& The timeline.
		 */
		detector_event_timeline& get() const noexcept;

	private:
		guarded_detector_event_timeline &m_timeline;
	};

	/**
	 * @brief Construct an empty timeline of a given rank.
	 *
	 * @param rank Number of coordinates each event carries.
	 * @throws std::invalid_argument If @p rank is zero.
	 */
	explicit guarded_detector_event_timeline(std::size_t rank);

	guarded_detector_event_timeline(
		const guarded_detector_event_timeline &other
	) = delete;
	guarded_detector_event_timeline(
		guarded_detector_event_timeline &&other
	) = delete;
	~guarded_detector_event_timeline() = default;

	guarded_detector_event_timeline&
	operator=(const guarded_detector_event_timeline &other) = delete;
	guarded_detector_event_timeline&
	operator=(guarded_detector_event_timeline &&other) = delete;

	/**
	 * @brief Count a view of the memory handed to Python.
	 *
	 * @throws nanobind::buffer_error If the timeline is being changed.
	 */
	void add_view();

	/**
	 * @brief Stop counting a view that is gone.
	 */
	void remove_view() noexcept;

	/**
	 * @brief Get how many views of the memory are alive.
	 *
	 * @return std::size_t The number of views.
	 */
	std::size_t get_view_count() const noexcept;

private:
	detector_event_timeline m_timeline;
	std::size_t m_view_count;
	std::size_t m_reader_count;
	bool m_writing;
};

} // namespace vitrio
