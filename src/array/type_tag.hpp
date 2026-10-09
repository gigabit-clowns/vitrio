// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace vitrio
{

/**
 * @brief Carries a type to a function as an argument.
 *
 * @tparam T The type carried.
 */
template <typename T>
class type_tag
{
public:
	using type = T; ///< The type carried.
};

} // namespace vitrio
