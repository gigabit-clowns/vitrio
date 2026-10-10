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
	const auto coordinates = events.get_coordinates();

	for (std::size_t first = 0; first < coordinates.size(); first += rank)
	{
		std::size_t pixel = 0;
		for (std::size_t axis = 0; axis < rank; ++axis)
		{
			const std::size_t coordinate = coordinates[first + axis];
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
	span<const std::size_t> bin_extents
)
	: m_bin_extents(bin_extents.begin(), bin_extents.end())
{
	if (m_bin_extents.empty())
	{
		throw std::invalid_argument(
			"counting_detector_event_renderer: The bin extents must not be "
			"empty."
		);
	}
	if (std::find(m_bin_extents.cbegin(), m_bin_extents.cend(), 0) !=
		m_bin_extents.cend())
	{
		throw std::invalid_argument(
			"counting_detector_event_renderer: A bin extent is zero."
		);
	}
}

counting_detector_event_renderer::~counting_detector_event_renderer() =
	default;

span<const std::size_t>
counting_detector_event_renderer::get_bin_extents() const noexcept
{
	return make_span(m_bin_extents.data(), m_bin_extents.size());
}

std::vector<std::size_t> counting_detector_event_renderer::get_image_extents(
	const detector_event_timeline_descriptor &events
) const
{
	if (events.get_rank() != m_bin_extents.size())
	{
		throw std::invalid_argument(
			"counting_detector_event_renderer: The events do not have the "
			"rank of the bin."
		);
	}

	const auto grid_extents = events.get_extents();
	std::vector<std::size_t> extents(grid_extents.size());
	for (std::size_t axis = 0; axis < extents.size(); ++axis)
	{
		const auto grid = grid_extents[axis];
		const auto bin = m_bin_extents[axis];
		extents[axis] = grid / bin + (grid % bin != 0 ? 1 : 0);
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

	std::vector<std::uint32_t> counts(pixel_count, 0);
	count_events(
		events,
		descriptor.get_extents(),
		get_bin_extents(),
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
