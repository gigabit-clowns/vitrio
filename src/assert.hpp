// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cassert>

/**
 * @brief Check a condition the code relies on, in debug builds only.
 */
#define VITRIO_ASSERT(expr) assert(expr)
