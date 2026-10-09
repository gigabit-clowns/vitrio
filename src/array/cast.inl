// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "cast.hpp"

#include <assert.hpp>

namespace vitrio
{

template <typename T, typename Q>
inline void cast(T *destination, const Q *source) noexcept
{
	VITRIO_ASSERT(destination);
	VITRIO_ASSERT(source);
	*destination = static_cast<T>(*source);
}

template <typename Q>
inline typename std::enable_if<std::is_arithmetic<Q>::value>::type
cast(float16_t *destination, const Q *source) noexcept
{
	VITRIO_ASSERT(destination);
	VITRIO_ASSERT(source);
	*destination = float16_t(static_cast<float>(*source));
}

template <typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value>::type
cast(T *destination, const float16_t *source) noexcept
{
	VITRIO_ASSERT(destination);
	VITRIO_ASSERT(source);
	*destination = static_cast<T>(static_cast<float>(*source));
}

template <typename T, typename Q>
inline void cast(
	std::complex<T> *destination,
	const std::complex<Q> *source
) noexcept
{
	VITRIO_ASSERT(destination);
	VITRIO_ASSERT(source);

	const Q source_real = source->real();
	const Q source_imag = source->imag();
	T real;
	T imag;
	cast(&real, &source_real);
	cast(&imag, &source_imag);

	*destination = std::complex<T>(real, imag);
}

template <typename T, typename Q>
inline void cast(std::complex<T> *destination, const Q *source) noexcept
{
	VITRIO_ASSERT(destination);

	T real;
	cast(&real, source);

	*destination = std::complex<T>(real);
}

} // namespace vitrio
