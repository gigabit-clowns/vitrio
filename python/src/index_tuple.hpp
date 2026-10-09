// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/span.hpp>

#include <nanobind/nanobind.h>

#include <cstddef>

namespace vitrio
{

/**
 * @brief The tuple Python is given for extents or for an index.
 *
 * It holds any number of integers, which is what its type says in the
 * signatures Python sees.
 */
using index_tuple =
	nanobind::typed<nanobind::tuple, std::size_t, nanobind::ellipsis>;

/**
 * @brief Make the tuple Python is given for extents or for an index.
 *
 * @param values The extents or the coordinates of the index.
 * @return index_tuple A tuple of integers, in the same order.
 */
index_tuple to_tuple(span<const std::size_t> values);

} // namespace vitrio
