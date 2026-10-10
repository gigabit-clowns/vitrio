// SPDX-License-Identifier: LGPL-2.1-or-later

#include <vitrio/detector_event_image_reader.hpp>

#include <formats/strided_transfer/image_page_regions.hpp>
#include <formats/strided_transfer/image_region_bounds.hpp>
#include <formats/strided_transfer/image_region_read_walk.hpp>
#include <formats/strided_transfer/image_region_transfer.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/array_ref.hpp>
#include <vitrio/detector_event_renderer.hpp>
#include <vitrio/detector_event_timeline.hpp>
#include <vitrio/detector_event_timeline_descriptor.hpp>
#include <vitrio/detector_event_timeline_reader.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/span.hpp>

#include <array/array_data.hpp>
#include <memory/byte_order.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace vitrio
{

namespace
{

const std::size_t image_rank = 2;

const std::size_t no_fraction = std::numeric_limits<std::size_t>::max();

const std::shared_ptr<const detector_event_timeline_reader>& check_events(
	const std::shared_ptr<const detector_event_timeline_reader> &events
)
{
	if (!events)
	{
		throw std::invalid_argument(
			"detector_event_image_reader: The events must not be null."
		);
	}
	return events;
}

const std::shared_ptr<const detector_event_renderer>& check_renderer(
	const std::shared_ptr<const detector_event_renderer> &renderer
)
{
	if (!renderer)
	{
		throw std::invalid_argument(
			"detector_event_image_reader: The renderer must not be null."
		);
	}
	return renderer;
}

std::vector<std::size_t> get_image_extents(
	const detector_event_timeline_reader &events,
	const detector_event_fractionation &fractionation,
	const detector_event_renderer &renderer
)
{
	const auto &descriptor = events.get_descriptor();
	if (fractionation.get_fraction_count() > descriptor.get_time_extent())
	{
		throw std::invalid_argument(
			"detector_event_image_reader: There are more fractions than time "
			"quanta to cut."
		);
	}

	auto extents = renderer.get_image_extents(descriptor);
	if (extents.size() != image_rank)
	{
		throw std::invalid_argument(
			"detector_event_image_reader: The renderer does not render "
			"images of rank two."
		);
	}
	return extents;
}

image_descriptor make_descriptor(
	const std::vector<std::size_t> &image_extents,
	const detector_event_fractionation &fractionation,
	numerical_type data_type
)
{
	std::vector<std::size_t> extents;
	if (fractionation.get_fraction_count() > 1)
	{
		extents.push_back(fractionation.get_fraction_count());
	}
	extents.insert(extents.end(), image_extents.cbegin(), image_extents.cend());

	return image_descriptor(
		make_span(extents.data(), extents.size()),
		image_rank,
		data_type
	);
}

} // anonymous namespace

class detector_event_image_reader::implementation
{
public:
	implementation(
		std::shared_ptr<const detector_event_timeline_reader> events,
		detector_event_fractionation fractionation,
		std::shared_ptr<const detector_event_renderer> renderer
	)
		: m_events(check_events(events))
		, m_fractionation(fractionation)
		, m_renderer(check_renderer(renderer))
		, m_image_extents(
			get_image_extents(*m_events, m_fractionation, *m_renderer))
		, m_descriptor(make_descriptor(
			m_image_extents,
			m_fractionation,
			m_renderer->get_data_type()
		))
		, m_metadata()
		, m_mutex()
		, m_timeline(m_events->get_descriptor().get_rank())
		, m_image(make_array(make_contiguous_array_descriptor(
			make_span(m_image_extents),
			m_renderer->get_data_type()
		)))
		, m_rendered_fraction(no_fraction)
	{
	}

	const detector_event_fractionation& get_fractionation() const noexcept
	{
		return m_fractionation;
	}

	const image_descriptor& get_descriptor() const noexcept
	{
		return m_descriptor;
	}

	const image_metadata& get_metadata() const noexcept
	{
		return m_metadata;
	}

	void read(array_ref destination, const image_transfer_plan &regions)
	{
		auto *array_data = get_array_data(destination);

		const auto &descriptor = destination.get_descriptor();
		const auto array_extents = descriptor.get_extents();
		const auto array_strides = descriptor.get_strides();

		check_region_bounds(
			regions,
			m_descriptor,
			array_extents,
			array_strides,
			descriptor.get_offset()
		);
		if (regions.get_region_count() == 0)
		{
			return;
		}

		const image_page_regions fractions(regions);

		const std::array<std::ptrdiff_t, image_rank> image_strides = {{
			static_cast<std::ptrdiff_t>(m_image_extents[1]),
			1
		}};

		const std::lock_guard<std::mutex> lock(m_mutex);

		const auto fraction_count = fractions.get_page_count();
		for (std::size_t position = 0; position < fraction_count; ++position)
		{
			render(fractions.get_page(position));

			const image_region_read_walk walk(
				fractions.get_regions(position),
				make_span(m_image_extents),
				make_span(image_strides),
				array_extents,
				array_strides,
				descriptor.get_offset()
			);

			read_regions(
				walk,
				array_data,
				descriptor.get_data_type(),
				m_image.get_data(),
				m_descriptor.get_data_type(),
				get_system_byte_order()
			);
		}
	}

private:
	// Called with the lock held.
	void render(std::size_t fraction)
	{
		if (fraction == m_rendered_fraction)
		{
			return;
		}

		// What is left of a fraction a failure stopped halfway is no
		// fraction at all.
		m_rendered_fraction = no_fraction;

		const auto &events = m_events->get_descriptor();
		const auto time_extent = events.get_time_extent();
		m_events->read(
			m_fractionation.get_fraction_begin(fraction, time_extent),
			m_fractionation.get_fraction_begin(fraction + 1, time_extent),
			m_timeline
		);
		m_renderer->render(events, m_timeline, m_image);

		m_rendered_fraction = fraction;
	}

	std::shared_ptr<const detector_event_timeline_reader> m_events;
	detector_event_fractionation m_fractionation;
	std::shared_ptr<const detector_event_renderer> m_renderer;
	std::vector<std::size_t> m_image_extents;
	image_descriptor m_descriptor;
	image_metadata m_metadata;
	std::mutex m_mutex;
	detector_event_timeline m_timeline;
	array m_image;
	std::size_t m_rendered_fraction;
};

detector_event_image_reader::detector_event_image_reader(
	std::shared_ptr<const detector_event_timeline_reader> events,
	detector_event_fractionation fractionation,
	std::shared_ptr<const detector_event_renderer> renderer
)
	: m_implementation(std::make_unique<implementation>(
		std::move(events),
		fractionation,
		std::move(renderer)
	))
{
}

detector_event_image_reader::~detector_event_image_reader() = default;

const detector_event_fractionation&
detector_event_image_reader::get_fractionation() const noexcept
{
	return m_implementation->get_fractionation();
}

const image_descriptor&
detector_event_image_reader::get_descriptor() const noexcept
{
	return m_implementation->get_descriptor();
}

const image_metadata&
detector_event_image_reader::get_metadata() const noexcept
{
	return m_implementation->get_metadata();
}

void detector_event_image_reader::read(
	array_ref destination,
	const image_transfer_plan &regions
) const
{
	m_implementation->read(destination, regions);
}

} // namespace vitrio
