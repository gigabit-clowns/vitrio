// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_region_layout.hpp"

#include <vitrio/image_transfer_plan.hpp>

#include <assert.hpp>

#include <cstdlib>
#include <utility>

namespace vitrio
{

namespace
{

// A stride of zero says nothing about where an axis belongs, so it compares
// equal to any other and leaves the question to what is compared next.
int compare_strides(std::ptrdiff_t lhs, std::ptrdiff_t rhs) noexcept
{
	if (lhs == 0 || rhs == 0)
	{
		return 0;
	}

	const auto lhs_magnitude = std::abs(lhs);
	const auto rhs_magnitude = std::abs(rhs);
	if (lhs_magnitude < rhs_magnitude)
	{
		return -1;
	}
	if (lhs_magnitude > rhs_magnitude)
	{
		return 1;
	}
	return 0;
}

// Tells which of two axes belongs further in. The destination decides, the
// source breaks its ties and the extents break those of the source.
int compare_axes(
	const image_region_layout::axis &lhs,
	const image_region_layout::axis &rhs
) noexcept
{
	const auto by_destination = compare_strides(
		lhs.get_destination_stride(),
		rhs.get_destination_stride()
	);
	if (by_destination != 0)
	{
		return by_destination;
	}

	const auto by_source =
		compare_strides(lhs.get_source_stride(), rhs.get_source_stride());
	if (by_source != 0)
	{
		return by_source;
	}

	if (lhs.get_extent() < rhs.get_extent())
	{
		return -1;
	}
	if (lhs.get_extent() > rhs.get_extent())
	{
		return 1;
	}
	return 0;
}

// Two axes are one when stepping off the end of the inner one lands, on both
// sides, where a step of the outer one does.
bool are_contiguous(
	const image_region_layout::axis &inner,
	const image_region_layout::axis &outer
) noexcept
{
	const auto extent = static_cast<std::ptrdiff_t>(inner.get_extent());

	return
		extent * inner.get_destination_stride() ==
			outer.get_destination_stride() &&
		extent * inner.get_source_stride() == outer.get_source_stride();
}

bool try_merge_axes(
	image_region_layout::axis &inner,
	const image_region_layout::axis &outer
) noexcept
{
	// An axis of one element is never stepped along, so its strides mean
	// nothing and the other axis stands for both.
	if (inner.get_extent() == 1)
	{
		inner = outer;
		return true;
	}

	if (outer.get_extent() == 1 || are_contiguous(inner, outer))
	{
		inner = image_region_layout::axis(
			inner.get_extent() * outer.get_extent(),
			inner.get_destination_stride(),
			inner.get_source_stride()
		);
		return true;
	}

	return false;
}

} // anonymous namespace

image_region_layout::axis::axis(
	std::size_t extent,
	std::ptrdiff_t destination_stride,
	std::ptrdiff_t source_stride
) noexcept
	: m_extent(extent)
	, m_destination_stride(destination_stride)
	, m_source_stride(source_stride)
{
}

std::size_t image_region_layout::axis::get_extent() const noexcept
{
	return m_extent;
}

std::ptrdiff_t
image_region_layout::axis::get_destination_stride() const noexcept
{
	return m_destination_stride;
}

std::ptrdiff_t image_region_layout::axis::get_source_stride() const noexcept
{
	return m_source_stride;
}

image_region_layout::image_region_layout(
	const image_transfer_plan &regions,
	span<const std::ptrdiff_t> destination_strides,
	span<const std::ptrdiff_t> source_strides
)
{
	const auto &shape = regions.get_shape();
	const auto extents = shape.get_extents();
	const auto rank = shape.get_rank();

	// The extents cover the trailing axes of each side, so its leading
	// strides are skipped.
	const auto *destination =
		destination_strides.data() +
		shape.get_leading_rank(destination_strides.size());
	const auto *source =
		source_strides.data() + shape.get_leading_rank(source_strides.size());

	// The last axis comes first: it is the innermost one of a side laid out
	// in row-major order, and so the order to start from.
	m_axes.reserve(rank);
	for (auto index = rank; index > 0; --index)
	{
		m_axes.emplace_back(
			extents[index - 1],
			destination[index - 1],
			source[index - 1]
		);
	}

	sort_axes_by_locality();
	merge_contiguous_axes();
}

std::size_t image_region_layout::get_axis_count() const noexcept
{
	return m_axes.size();
}

const image_region_layout::axis&
image_region_layout::get_axis(std::size_t index) const noexcept
{
	VITRIO_ASSERT(index < m_axes.size());
	return m_axes[index];
}

std::size_t image_region_layout::compute_element_count() const noexcept
{
	std::size_t count = 1;
	for (const auto &item : m_axes)
	{
		count *= item.get_extent();
	}

	return count;
}

void image_region_layout::sort_axes_by_locality() noexcept
{
	// An insertion sort, since two axes may not be comparable: one that the
	// axis being inserted cannot be compared with is stepped over.
	for (std::size_t index = 1; index < m_axes.size(); ++index)
	{
		auto inserted = index;
		for (auto candidate = index; candidate > 0; --candidate)
		{
			const auto comparison =
				compare_axes(m_axes[candidate - 1], m_axes[inserted]);
			if (comparison > 0)
			{
				std::swap(m_axes[candidate - 1], m_axes[inserted]);
				inserted = candidate - 1;
			}
			else if (comparison < 0)
			{
				break;
			}
		}
	}
}

void image_region_layout::merge_contiguous_axes()
{
	if (m_axes.size() <= 1)
	{
		return;
	}

	// Each axis is merged into the last one kept, or kept after it.
	std::size_t kept = 0;
	for (std::size_t index = 1; index < m_axes.size(); ++index)
	{
		if (!try_merge_axes(m_axes[kept], m_axes[index]))
		{
			++kept;
			m_axes[kept] = m_axes[index];
		}
	}

	m_axes.erase(m_axes.begin() + kept + 1, m_axes.end());
}

} // namespace vitrio
