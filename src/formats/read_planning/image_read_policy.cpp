// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_read_policy.hpp"

#include <system/page_size.hpp>

#include <algorithm>

namespace vitrio
{

image_read_policy::image_read_policy(
	std::size_t gap_tolerance,
	std::size_t byte_budget,
	std::size_t alignment,
	std::size_t alignment_origin
) noexcept
	: m_gap_tolerance(gap_tolerance)
	, m_byte_budget(byte_budget)
	, m_alignment(alignment)
	, m_alignment_origin(alignment_origin)
{
}

std::size_t image_read_policy::get_gap_tolerance() const noexcept
{
	return m_gap_tolerance;
}

std::size_t image_read_policy::get_byte_budget() const noexcept
{
	return m_byte_budget;
}

std::size_t image_read_policy::get_alignment() const noexcept
{
	return m_alignment;
}

std::size_t image_read_policy::get_alignment_origin() const noexcept
{
	return m_alignment_origin;
}

image_read_policy make_prefetch_policy(std::size_t region_span) noexcept
{
	const auto page_size = get_page_size();
	const auto floor = std::max(region_span, page_size);

	return image_read_policy(
		std::min(floor, default_prefetch_gap_cap),
		default_prefetch_budget,
		page_size,
		0
	);
}

} // namespace vitrio
