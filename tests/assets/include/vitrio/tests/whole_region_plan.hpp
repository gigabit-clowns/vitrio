// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_transfer_shape.hpp>
#include <vitrio/span.hpp>

#include <cstddef>
#include <vector>

namespace vitrio
{
namespace test
{

/**
 * @brief Make the plan of one region that covers a whole file and a whole
 * array of the same extents.
 *
 * @param extents Extents of both.
 * @return image_transfer_plan The plan.
 */
inline image_transfer_plan whole_of(const std::vector<std::size_t> &extents)
{
	image_transfer_plan regions(
		image_transfer_shape(extents, extents.size(), extents.size())
	);
	const std::vector<std::size_t> origin(extents.size(), 0);
	regions.add(make_span(origin), make_span(origin));

	return regions;
}

} // namespace test
} // namespace vitrio
