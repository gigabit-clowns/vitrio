// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>

namespace vitrio
{

/**
 * @brief Call a function with the C++ type a numerical_type stands for.
 *
 * The type is handed over as a @ref type_tag, so the function is a generic
 * one that names it through the tag. It is instantiated for every type an
 * array may hold, whichever one is asked for.
 *
 * @tparam F Function taking a type_tag of any element type. It must return
 * the same type for every one of them.
 * @param visitor The function to call.
 * @param type The type to call it with.
 * @return What @p visitor returns.
 * @throws std::invalid_argument If @p type is unknown.
 */
template <typename F>
auto dispatch_numerical_type(F &&visitor, numerical_type type);

} // namespace vitrio

#include "numerical_type_dispatch.inl"
