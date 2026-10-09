// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "span.hpp"

namespace vitrio
{

template <typename T>
constexpr span<T>::span() noexcept
	: m_data(nullptr)
	, m_size(0)
{
}

template <typename T>
constexpr span<T>::span(pointer data, size_type size) noexcept
	: m_data(data)
	, m_size(size)
{
}

template <typename T>
template <typename U, typename>
constexpr span<T>::span(const span<U> &other) noexcept
	: m_data(other.data())
	, m_size(other.size())
{
}

template <typename T>
constexpr typename span<T>::pointer span<T>::data() const noexcept
{
	return m_data;
}

template <typename T>
constexpr typename span<T>::size_type span<T>::size() const noexcept
{
	return m_size;
}

template <typename T>
constexpr bool span<T>::empty() const noexcept
{
	return m_size == 0;
}

template <typename T>
constexpr typename span<T>::reference
span<T>::operator[](size_type index) const noexcept
{
	return m_data[index];
}

template <typename T>
constexpr typename span<T>::reference span<T>::front() const noexcept
{
	return *m_data;
}

template <typename T>
constexpr typename span<T>::reference span<T>::back() const noexcept
{
	return m_data[m_size - 1];
}

template <typename T>
constexpr typename span<T>::iterator span<T>::begin() const noexcept
{
	return m_data;
}

template <typename T>
constexpr typename span<T>::iterator span<T>::end() const noexcept
{
	return m_data + m_size;
}

template <typename T>
constexpr span<T> make_span(T *data, std::size_t size) noexcept
{
	return span<T>(data, size);
}

template <typename T>
inline span<T> make_span(std::vector<T> &values) noexcept
{
	return span<T>(values.data(), values.size());
}

template <typename T>
inline span<const T> make_span(const std::vector<T> &values) noexcept
{
	return span<const T>(values.data(), values.size());
}

template <typename T, std::size_t N>
inline span<T> make_span(std::array<T, N> &values) noexcept
{
	return span<T>(values.data(), values.size());
}

template <typename T, std::size_t N>
inline span<const T> make_span(const std::array<T, N> &values) noexcept
{
	return span<const T>(values.data(), values.size());
}

} // namespace vitrio
