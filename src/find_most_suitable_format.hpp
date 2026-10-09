// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_file_format_suitability.hpp>

namespace vitrio
{

/**
 * @brief Find the item of a range that reports the highest suitability.
 *
 * Among items reporting the same suitability, the first one is chosen.
 *
 * @tparam ForwardIte Forward iterator.
 * @tparam F Function taking an item of the range and returning its
 * image_file_format_suitability.
 * @param first First item of the range.
 * @param last Past the last item of the range.
 * @param suitability_evaluator Function that evaluates each item.
 * @return ForwardIte The most suitable item. @p last if every item is
 * unsupported.
 */
template <typename ForwardIte, typename F>
ForwardIte find_most_suitable_format(
	ForwardIte first,
	ForwardIte last,
	const F &suitability_evaluator
);

} // namespace vitrio

#include "find_most_suitable_format.inl"
