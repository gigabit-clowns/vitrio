// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <array>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace vitrio
{

/**
 * @brief A view of a contiguous sequence of elements.
 *
 * A span refers to elements it does not own, through a pointer to the first
 * one and a count. Copying a span copies the reference and not the elements.
 * The elements must outlive every span that refers to them.
 *
 * A span of a const type gives read-only access to its elements. A span of a
 * non-const type converts to it.
 *
 * @tparam T Type of the elements. It may be const.
 */
template <typename T>
class span
{
public:
	using element_type = T;
	using value_type = typename std::remove_cv<T>::type;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using pointer = T*;
	using reference = T&;
	using iterator = T*;

	/**
	 * @brief Construct a span of no elements.
	 */
	constexpr span() noexcept;

	/**
	 * @brief Construct a span from its first element and its size.
	 *
	 * @param data Pointer to the first element.
	 * @param size Number of elements.
	 */
	constexpr span(pointer data, size_type size) noexcept;

	/**
	 * @brief Construct a span from one of a less qualified element type.
	 *
	 * This is what converts a span of non-const elements into one of const
	 * elements.
	 *
	 * @tparam U Element type of the other span.
	 * @param other The span to refer to the elements of.
	 */
	template <
		typename U,
		typename = typename std::enable_if<
			std::is_convertible<U (*)[], T (*)[]>::value
		>::type
	>
	constexpr span(const span<U> &other) noexcept;

	span(const span &other) = default;
	~span() = default;

	span& operator=(const span &other) = default;

	/**
	 * @brief Get the first element.
	 *
	 * @return pointer Pointer to the first element.
	 */
	constexpr pointer data() const noexcept;

	/**
	 * @brief Get the number of elements.
	 *
	 * @return size_type The number of elements.
	 */
	constexpr size_type size() const noexcept;

	/**
	 * @brief Check whether the span has no elements.
	 *
	 * @return true The span has no elements.
	 * @return false The span has at least one element.
	 */
	constexpr bool empty() const noexcept;

	/**
	 * @brief Get one element.
	 *
	 * @param index Position of the element. Must be below @ref size.
	 * @return reference The element.
	 */
	constexpr reference operator[](size_type index) const noexcept;

	/**
	 * @brief Get the first element.
	 *
	 * The span must not be empty.
	 *
	 * @return reference The first element.
	 */
	constexpr reference front() const noexcept;

	/**
	 * @brief Get the last element.
	 *
	 * The span must not be empty.
	 *
	 * @return reference The last element.
	 */
	constexpr reference back() const noexcept;

	/**
	 * @brief Get an iterator to the first element.
	 *
	 * @return iterator The iterator.
	 */
	constexpr iterator begin() const noexcept;

	/**
	 * @brief Get an iterator past the last element.
	 *
	 * @return iterator The iterator.
	 */
	constexpr iterator end() const noexcept;

private:
	pointer m_data;
	size_type m_size;
};

/**
 * @brief Make a span from its first element and its size.
 *
 * @tparam T Type of the elements. It may be const.
 * @param data Pointer to the first element.
 * @param size Number of elements.
 * @return span<T> The span.
 */
template <typename T>
constexpr span<T> make_span(T *data, std::size_t size) noexcept;

/**
 * @brief Make a span of the elements of a vector.
 *
 * @tparam T Type of the elements.
 * @param values The vector. Changing its size invalidates the span.
 * @return span<T> The span.
 */
template <typename T>
span<T> make_span(std::vector<T> &values) noexcept;

/**
 * @brief Make a read-only span of the elements of a vector.
 *
 * @tparam T Type of the elements.
 * @param values The vector. Changing its size invalidates the span.
 * @return span<const T> The span.
 */
template <typename T>
span<const T> make_span(const std::vector<T> &values) noexcept;

/**
 * @brief Make a span of the elements of an array.
 *
 * @tparam T Type of the elements.
 * @tparam N Number of elements.
 * @param values The array.
 * @return span<T> The span.
 */
template <typename T, std::size_t N>
span<T> make_span(std::array<T, N> &values) noexcept;

/**
 * @brief Make a read-only span of the elements of an array.
 *
 * @tparam T Type of the elements.
 * @tparam N Number of elements.
 * @param values The array.
 * @return span<const T> The span.
 */
template <typename T, std::size_t N>
span<const T> make_span(const std::array<T, N> &values) noexcept;

} // namespace vitrio

#include "span.inl"
