// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "image_metadata_value.hpp"

namespace vitrio
{

template <
	typename T,
	typename std::enable_if<
		std::is_integral<T>::value && !std::is_same<T, bool>::value,
		int
	>::type
>
inline image_metadata_value::image_metadata_value(T value)
	: m_type(image_metadata_value_type::integer)
	, m_integer(to_integer(value, typename std::is_signed<T>::type()))
	, m_real(0.0)
	, m_text()
{
}

template <
	typename T,
	typename std::enable_if<
		std::is_floating_point<T>::value,
		int
	>::type
>
inline image_metadata_value::image_metadata_value(T value) noexcept
	: m_type(image_metadata_value_type::real)
	, m_integer(0)
	, m_real(static_cast<double>(value))
	, m_text()
{
}

template <typename T>
inline std::int64_t image_metadata_value::to_integer(
	T value,
	std::true_type /*is_signed*/
) noexcept
{
	// No signed type of the language is wider.
	return static_cast<std::int64_t>(value);
}

template <typename T>
inline std::int64_t image_metadata_value::to_integer(
	T value,
	std::false_type /*is_signed*/
)
{
	return to_integer(static_cast<std::uintmax_t>(value));
}

} // namespace vitrio
