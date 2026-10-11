// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/counting_detector_event_renderer.hpp>

#include <formats/strided_transfer/image_region_read_walk.hpp>
#include <formats/strided_transfer/image_region_transfer.hpp>

#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/detector_event_timeline.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/span.hpp>

#include <array/array_data.hpp>
#include <memory/byte_order.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace vitrio
{

namespace
{

std::vector<std::ptrdiff_t> make_contiguous_strides(
	const std::vector<std::size_t> &extents
)
{
	std::vector<std::ptrdiff_t> strides(extents.size());

	std::ptrdiff_t stride = 1;
	for (auto axis = extents.size(); axis > 0; --axis)
	{
		strides[axis - 1] = stride;
		stride *= static_cast<std::ptrdiff_t>(extents[axis - 1]);
	}

	return strides;
}

void count_events(
	const detector_event_timeline &events,
	span<const std::size_t> grid_extents,
	span<const std::size_t> bin_extents,
	const std::vector<std::size_t> &image_extents,
	std::vector<std::uint32_t> &counts
)
{
	const auto rank = image_extents.size();
	const auto positions = events.get_positions();
	const auto event_count = positions.get_event_count();

	for (std::size_t event = 0; event < event_count; ++event)
	{
		std::size_t pixel = 0;
		for (std::size_t axis = 0; axis < rank; ++axis)
		{
			const std::size_t coordinate = positions(event, axis);
			if (coordinate >= grid_extents[axis])
			{
				throw std::out_of_range(
					"counting_detector_event_renderer::render: An event "
					"lies outside the grid."
				);
			}
			pixel = pixel * image_extents[axis] +
				coordinate / bin_extents[axis];
		}

		auto &count = counts[pixel];
		if (count == std::numeric_limits<std::uint32_t>::max())
		{
			throw std::overflow_error(
				"counting_detector_event_renderer::render: A pixel gathers "
				"more events than can be counted."
			);
		}
		++count;
	}
}

} // anonymous namespace

counting_detector_event_renderer::counting_detector_event_renderer(
	detector_event_binning binning
) noexcept
	: m_binning(binning)
{
}

counting_detector_event_renderer::~counting_detector_event_renderer() =
	default;

const detector_event_binning&
counting_detector_event_renderer::get_binning() const noexcept
{
	return m_binning;
}

std::vector<std::size_t> counting_detector_event_renderer::get_image_extents(
	const detector_event_timeline_descriptor &events
) const
{
	auto extents = m_binning.resolve(events);
	const auto grid_extents = events.get_extents();
	for (std::size_t axis = 0; axis < extents.size(); ++axis)
	{
		extents[axis] = grid_extents[axis] / extents[axis];
	}

	return extents;
}

numerical_type
counting_detector_event_renderer::get_data_type() const noexcept
{
	return numerical_type::uint32;
}

void counting_detector_event_renderer::render(
	const detector_event_timeline_descriptor &descriptor,
	const detector_event_timeline &events,
	array_ref destination
) const
{
	const auto image_extents = get_image_extents(descriptor);
	if (events.get_rank() != descriptor.get_rank())
	{
		throw std::invalid_argument(
			"counting_detector_event_renderer::render: The events do not "
			"have the rank of the descriptor."
		);
	}

	const auto &array = destination.get_descriptor();
	if (!is_initialized(array))
	{
		throw std::invalid_argument(
			"counting_detector_event_renderer::render: The destination is "
			"not initialized."
		);
	}
	const auto array_extents = array.get_extents();
	if (!std::equal(
			array_extents.begin(),
			array_extents.end(),
			image_extents.cbegin(),
			image_extents.cend()))
	{
		throw std::invalid_argument(
			"counting_detector_event_renderer::render: The destination does "
			"not have the extents of an image."
		);
	}

	// The extents are those of the destination, which already holds as
	// many elements, so their product does not overflow.
	std::size_t pixel_count = 1;
	for (const auto extent : image_extents)
	{
		pixel_count *= extent;
	}

	const auto bin_extents = m_binning.resolve(descriptor);
	std::vector<std::uint32_t> counts(pixel_count, 0);
	count_events(
		events,
		descriptor.get_extents(),
		make_span(bin_extents),
		image_extents,
		counts
	);

	// The counts are moved into the destination as a file whose single
	// region is all of it, which converts them to its data type and walks
	// its strides.
	const auto rank = image_extents.size();
	image_transfer_plan plan(image_transfer_shape(image_extents, rank, rank));
	const std::vector<std::size_t> origin(rank, 0);
	plan.add(make_span(origin), make_span(origin));

	const auto count_strides = make_contiguous_strides(image_extents);
	const image_region_read_walk walk(
		plan,
		make_span(image_extents),
		make_span(count_strides),
		array_extents,
		array.get_strides(),
		array.get_offset()
	);

	read_regions(
		walk,
		get_array_data(destination),
		array.get_data_type(),
		reinterpret_cast<const byte*>(counts.data()),
		numerical_type::uint32,
		get_system_byte_order()
	);
}

} // namespace vitrio
