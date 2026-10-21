// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_read_schedule.hpp"

#include <vitrio/image_transfer_plan.hpp>

#include <assert.hpp>

#include <algorithm>

namespace vitrio
{

std::size_t compute_region_span(
	const image_transfer_plan &regions,
	span<const std::ptrdiff_t> file_strides,
	numerical_type data_type
) noexcept
{
	const auto &shape = regions.get_shape();
	const auto rank = file_strides.size();

	std::size_t elements = 1;
	for (std::size_t axis = 0; axis < rank; ++axis)
	{
		const auto extent = shape.get_extent(rank, axis);
		if (extent == 0)
		{
			return 0;
		}

		VITRIO_ASSERT(file_strides[axis] >= 0);
		elements +=
			(extent - 1) * static_cast<std::size_t>(file_strides[axis]);
	}

	return elements * get_size(data_type);
}

namespace
{

file_range make_range(std::size_t begin, std::size_t end) noexcept
{
	return file_range(begin, end - begin);
}

// The stretch one region occupies: grown back to the line of the grid its
// first byte falls on or after, and cut short where the file ends.
file_range locate_region(
	std::size_t begin,
	std::size_t region_span,
	std::size_t file_size,
	const image_read_policy &policy
) noexcept
{
	const auto alignment = policy.get_alignment();
	const auto origin = policy.get_alignment_origin();
	VITRIO_ASSERT(begin >= origin);

	return make_range(
		begin - (begin - origin) % alignment,
		std::min(begin + region_span, file_size)
	);
}

file_range merge(const file_range &previous, const file_range &next) noexcept
{
	return make_range(
		previous.get_offset(),
		std::max(previous.get_end(), next.get_end())
	);
}

// Whether a stretch is taken together with the one before it: it starts
// within the tolerance of where that one ends, and taking it in does not grow
// that one past the budget of a step. One already past the budget, a single
// region wider than it, still takes in what lies within it.
bool joins(
	const file_range &previous,
	const file_range &next,
	const image_read_policy &policy
) noexcept
{
	if (next.get_offset() > previous.get_end() + policy.get_gap_tolerance())
	{
		return false;
	}

	const auto merged_size = merge(previous, next).get_size();

	return merged_size <=
		std::max(policy.get_byte_budget(), previous.get_size());
}

} // anonymous namespace

image_read_schedule::image_read_schedule(
	const image_transfer_plan &regions,
	span<const std::ptrdiff_t> file_strides,
	numerical_type data_type,
	span<const std::ptrdiff_t> file_offsets,
	std::size_t data_offset,
	std::size_t file_size,
	const image_read_policy &policy
)
{
	VITRIO_ASSERT(policy.get_alignment() > 0);
	VITRIO_ASSERT(policy.get_alignment_origin() < policy.get_alignment());
	VITRIO_ASSERT(
		std::is_sorted(file_offsets.begin(), file_offsets.end())
	);

	m_step_first_range.push_back(0);
	m_step_first_region.push_back(0);

	if (file_offsets.empty())
	{
		return;
	}

	gather_ranges(
		regions,
		file_strides,
		data_type,
		file_offsets,
		data_offset,
		file_size,
		policy
	);

	gather_steps(file_offsets.size(), policy.get_byte_budget());
}

void image_read_schedule::gather_ranges(
	const image_transfer_plan &regions,
	span<const std::ptrdiff_t> file_strides,
	numerical_type data_type,
	span<const std::ptrdiff_t> file_offsets,
	std::size_t data_offset,
	std::size_t file_size,
	const image_read_policy &policy
)
{
	const auto region_span =
		compute_region_span(regions, file_strides, data_type);
	if (region_span == 0)
	{
		return;
	}

	const auto element_size = get_size(data_type);
	for (const auto offset : file_offsets)
	{
		VITRIO_ASSERT(offset >= 0);
		const auto begin =
			data_offset + static_cast<std::size_t>(offset) * element_size;
		if (begin >= file_size)
		{
			continue;
		}

		const auto region =
			locate_region(begin, region_span, file_size, policy);

		if (!m_ranges.empty() && joins(m_ranges.back(), region, policy))
		{
			m_ranges.back() = merge(m_ranges.back(), region);
			++m_regions_per_range.back();
		}
		else
		{
			m_ranges.push_back(region);
			m_regions_per_range.push_back(1);
		}
	}
}

void image_read_schedule::gather_steps(
	std::size_t region_count,
	std::size_t byte_budget
)
{
	std::size_t step_bytes = 0;
	std::size_t regions_seen = 0;
	for (std::size_t i = 0; i < m_ranges.size(); ++i)
	{
		const auto size = m_ranges[i].get_size();
		const auto opens_step = i == m_step_first_range.back();
		if (!opens_step && step_bytes + size > byte_budget)
		{
			m_step_first_range.push_back(i);
			m_step_first_region.push_back(regions_seen);
			step_bytes = 0;
		}

		step_bytes += size;
		regions_seen += m_regions_per_range[i];
	}

	// The last step takes every region the stretches left out, which is what
	// makes the steps tile the regions however little there was to take.
	m_step_first_range.push_back(m_ranges.size());
	m_step_first_region.push_back(region_count);
}

std::size_t image_read_schedule::get_step_count() const noexcept
{
	return m_step_first_range.size() - 1;
}

span<const file_range> image_read_schedule::get_ranges() const noexcept
{
	return make_span(m_ranges.data(), m_ranges.size());
}

span<const file_range>
image_read_schedule::get_step_ranges(std::size_t step) const noexcept
{
	VITRIO_ASSERT(step < get_step_count());

	const auto first = m_step_first_range[step];

	return make_span(
		m_ranges.data() + first,
		m_step_first_range[step + 1] - first
	);
}

std::size_t
image_read_schedule::get_step_first_range(std::size_t step) const noexcept
{
	VITRIO_ASSERT(step < get_step_count());

	return m_step_first_range[step];
}

std::size_t
image_read_schedule::get_step_first_region(std::size_t step) const noexcept
{
	VITRIO_ASSERT(step < get_step_count());

	return m_step_first_region[step];
}

std::size_t
image_read_schedule::get_step_region_count(std::size_t step) const noexcept
{
	VITRIO_ASSERT(step < get_step_count());

	return m_step_first_region[step + 1] - m_step_first_region[step];
}

span<const std::size_t>
image_read_schedule::get_regions_per_range() const noexcept
{
	return make_span(m_regions_per_range.data(), m_regions_per_range.size());
}

} // namespace vitrio
