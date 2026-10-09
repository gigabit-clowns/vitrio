// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/tests/host_array.hpp>

#include <formats/memory_mapping/image_file_layout.hpp>
#include <formats/memory_mapping/mapped_image_reader.hpp>
#include <memory/byte_order.hpp>

#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace vitrio
{
namespace test
{

/**
 * @brief Make the float32 values first, first + 1, first + 2 and onwards.
 *
 * @param first The first value.
 * @param count Number of values.
 * @return std::vector<float> The values.
 */
inline std::vector<float> count_from(std::size_t first, std::size_t count)
{
	std::vector<float> values(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		values[i] = static_cast<float>(first + i);
	}

	return values;
}

/**
 * @brief Write a headerless file of float32 values that count up from zero.
 *
 * The values are in the byte order of the host. The value of an element is
 * its linear index, so an image file of extents (N, H, W) holds the value
 * (n * H + h) * W + w at (n, h, w).
 *
 * @param path Path to the file. It is replaced if it exists.
 * @param extents Extents of the image the file holds.
 */
inline void write_counting_image_file(
	const std::string &path,
	const std::vector<std::size_t> &extents
)
{
	const auto values = count_from(0, count_elements(extents));

	std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
	output.write(
		reinterpret_cast<const char*>(values.data()),
		static_cast<std::streamsize>(values.size() * sizeof(float))
	);
}

/**
 * @brief Open a file written by @ref write_counting_image_file.
 *
 * @param path Path to the file.
 * @param extents Extents of the image the file holds.
 * @param core_rank Number of trailing extents that are one image.
 * @return std::shared_ptr<const image_reader> A reader over the file.
 */
inline std::shared_ptr<const image_reader> open_counting_image_file(
	const std::string &path,
	const std::vector<std::size_t> &extents,
	std::size_t core_rank
)
{
	std::vector<std::ptrdiff_t> strides(extents.size());
	std::ptrdiff_t stride = 1;
	for (auto axis = extents.size(); axis > 0; --axis)
	{
		strides[axis - 1] = stride;
		stride *= static_cast<std::ptrdiff_t>(extents[axis - 1]);
	}

	return std::make_shared<mapped_image_reader>(
		path,
		image_file_layout(
			image_descriptor(
				make_span(extents),
				core_rank,
				numerical_type::float32
			),
			std::move(strides),
			0,
			get_system_byte_order()
		),
		image_metadata()
	);
}

} // namespace test
} // namespace vitrio
