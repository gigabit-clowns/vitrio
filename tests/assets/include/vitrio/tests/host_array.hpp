// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/array.hpp>
#include <vitrio/array/array_descriptor.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/span.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <numeric>
#include <vector>

namespace vitrio
{
namespace test
{

/**
 * @brief Get the number of elements of an array.
 *
 * @param extents Extents of the array.
 * @return std::size_t The product of the extents.
 */
inline std::size_t count_elements(const std::vector<std::size_t> &extents)
{
	return std::accumulate(
		extents.cbegin(),
		extents.cend(),
		std::size_t(1),
		std::multiplies<std::size_t>()
	);
}

/**
 * @brief Make a contiguous array whose elements all hold one value.
 *
 * @tparam T Element type. Must match @p data_type.
 * @param extents Extents of the array.
 * @param data_type Data type of the array.
 * @param value The value of every element.
 * @return array The array.
 */
template <typename T>
array make_host_array(
	const std::vector<std::size_t> &extents,
	numerical_type data_type,
	T value
)
{
	auto result = make_array(
		make_contiguous_array_descriptor(make_span(extents), data_type)
	);
	std::fill_n(
		reinterpret_cast<T*>(result.get_data()),
		count_elements(extents),
		value
	);

	return result;
}

/**
 * @brief Make a contiguous array whose elements are all zero.
 *
 * @tparam T Element type. Must match @p data_type.
 * @param extents Extents of the array.
 * @param data_type Data type of the array.
 * @return array The array.
 */
template <typename T>
array make_host_array(
	const std::vector<std::size_t> &extents,
	numerical_type data_type
)
{
	return make_host_array(extents, data_type, T(0));
}

/**
 * @brief Make a contiguous array that holds given values.
 *
 * @tparam T Element type. Must match @p data_type.
 * @param extents Extents of the array.
 * @param data_type Data type of the array.
 * @param values The values, as many as @p extents hold and in the order
 * they are stored.
 * @return array The array.
 */
template <typename T>
array make_host_array(
	const std::vector<std::size_t> &extents,
	numerical_type data_type,
	const std::vector<T> &values
)
{
	auto result = make_array(
		make_contiguous_array_descriptor(make_span(extents), data_type)
	);
	std::copy(
		values.cbegin(),
		values.cend(),
		reinterpret_cast<T*>(result.get_data())
	);

	return result;
}

/**
 * @brief Copy the elements of a contiguous array.
 *
 * @tparam T Element type. Must match the data type of @p values.
 * @param values The array.
 * @return std::vector<T> Its elements, in the order they are stored.
 */
template <typename T>
std::vector<T> get_values(const array &values)
{
	const auto &descriptor = values.get_descriptor();
	const auto extents = descriptor.get_extents();
	const auto *data =
		reinterpret_cast<const T*>(values.get_data()) + descriptor.get_offset();
	const auto count = count_elements(
		std::vector<std::size_t>(extents.begin(), extents.end())
	);

	return std::vector<T>(data, data + count);
}

} // namespace test
} // namespace vitrio
