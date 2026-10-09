// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "find_most_suitable_format.hpp"

#include <utility>

namespace vitrio
{

template <typename ForwardIte, typename F>
inline
ForwardIte find_most_suitable_format(
	ForwardIte first,
	ForwardIte last,
	const F& suitability_evaluator
)
{
	std::pair<ForwardIte, image_file_format_suitability> best(
		last,
		image_file_format_suitability::unsupported
	);

	for (auto ite = first; ite != last; ++ite)
	{
		const auto suitability = suitability_evaluator(*ite);
		if (suitability > best.second)
		{
			best = { ite, suitability };
		}
	}

	return best.first;
}

} // namespace vitrio
