// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/export.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{

class array_ref;

class detector_event_timeline;
class detector_event_timeline_descriptor;

/**
 * @brief Abstract way of turning the events of a detector into an image.
 *
 * A renderer is given events and fills an image with them, whatever the time
 * they span: which events make an image is decided by whoever hands them
 * over. What an image is made of, be it a count of the events per pixel or
 * anything worked out of them, is the business of the renderer, as are its
 * extents and the data type it is worked out in.
 *
 * @par Thread safety
 * Every method may be called concurrently.
 *
 * @see detector_event_timeline
 */
class VITRIO_API detector_event_renderer
{
public:
	detector_event_renderer() noexcept;
	detector_event_renderer(const detector_event_renderer &other) = delete;
	detector_event_renderer(detector_event_renderer &&other) = delete;
	virtual ~detector_event_renderer();

	detector_event_renderer&
	operator=(const detector_event_renderer &other) = delete;
	detector_event_renderer&
	operator=(detector_event_renderer &&other) = delete;

	/**
	 * @brief Get the extents of the images rendered from an acquisition.
	 *
	 * @param events The grid the events are placed on.
	 * @return std::vector<std::size_t> The extents of an image, slowest axis
	 * first.
	 * @throws std::invalid_argument If this renderer can not render events
	 * placed on the grid of @p events.
	 */
	virtual std::vector<std::size_t> get_image_extents(
		const detector_event_timeline_descriptor &events
	) const = 0;

	/**
	 * @brief Get the data type images are worked out in.
	 *
	 * Rendering into an image of that type converts nothing, which makes it
	 * the cheapest to ask for.
	 *
	 * @return numerical_type The data type. Never unknown.
	 */
	virtual numerical_type get_data_type() const noexcept = 0;

	/**
	 * @brief Render events into an image.
	 *
	 * Every element of @p destination is written, whatever it held before.
	 * Values are converted to its data type as a cast between the two types
	 * does.
	 *
	 * @param descriptor The grid the events are placed on and the time they
	 * span.
	 * @param events The events to render. Must have the rank of
	 * @p descriptor, and every coordinate must lie within its extents.
	 * @param destination Where the image is written. Must be initialized and
	 * have the extents @ref get_image_extents gives for @p descriptor. It
	 * may be strided.
	 * @throws std::invalid_argument If this renderer can not render events
	 * placed on the grid of @p descriptor, if @p events does not have its
	 * rank, or if @p destination is not initialized or does not have the
	 * extents of an image.
	 * @throws std::out_of_range If an event lies outside the grid of
	 * @p descriptor.
	 * @throws unsupported_operation_error If the data type of
	 * @p destination can not be produced from that of @ref get_data_type.
	 */
	virtual void render(
		const detector_event_timeline_descriptor &descriptor,
		const detector_event_timeline &events,
		array_ref destination
	) const = 0;
};

} // namespace vitrio
