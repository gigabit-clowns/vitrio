// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_region_layout.hpp"

#include <vitrio/span.hpp>

#include <assert.hpp>

#include <cstddef>
#include <type_traits>

namespace vitrio
{

/**
 * @brief The stride of a side that is contiguous along the innermost axis.
 *
 * It stands in for a @c std::ptrdiff_t of one, known when the code is
 * compiled. Every other stride is handed over as a @c std::ptrdiff_t.
 */
using region_unit_stride = std::integral_constant<std::ptrdiff_t, 1>;

// The loop is a body, image_region_loop_impl.hpp, compiled here into vitrio
// for every translation unit. A unit that compiles it once more, under
// compiler attributes of its own, includes this header and then the body
// again inside another namespace.
#include "image_region_loop_impl.hpp"

} // namespace vitrio
