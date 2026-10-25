// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_descriptor.hpp>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief Get the name version 3 of ZARR gives a data type.
 *
 * @param type The data type.
 * @return std::string The name, such as "float32".
 * @throws unsupported_operation_error If ZARR has no such type: a complex
 * number of half precision.
 */
std::string get_v3_data_type_name(numerical_type type);

/**
 * @brief Choose the extents of a chunk an image is written in.
 *
 * Each image or volume of a stack starts chunks of its own, a chunk one of
 * them deep along the axes of the stack. An image is chunked in squares, and
 * a volume in cubes, as wide as given or as the image is.
 *
 * @param descriptor What is written.
 * @param image_edge The width of a chunk of an image of two axes.
 * @param volume_edge The width of a chunk of a volume.
 * @return std::vector<std::size_t> The extents of a chunk, none of them zero.
 */
std::vector<std::size_t> choose_chunk_extents(
	const image_descriptor &descriptor,
	std::size_t image_edge,
	std::size_t volume_edge
);

/**
 * @brief Name the axes of an image written to ZARR.
 *
 * The axes of an image or volume are z, y and x, its last that of x, and an
 * axis of the stack it is in is n, or n0, n1 and on when there are several.
 *
 * @param descriptor What is written.
 * @return std::vector<std::string> One name per axis.
 */
std::vector<std::string> name_axes(const image_descriptor &descriptor);

/**
 * @brief Tell whether an image written to ZARR is an image of OME-Zarr too.
 *
 * OME-Zarr allows from two to five axes, of which two or three are of
 * space and trail the others.
 *
 * @param descriptor What is written.
 * @return bool Whether OME-Zarr can state it.
 */
bool is_ome_image(const image_descriptor &descriptor);

/**
 * @brief Make the metadata of an array of version 3 that holds an image, as
 * vitrio writes it.
 *
 * Its chunks are named by the default encoding, in directories, encoded as
 * bytes little endian and compressed by blosc with zstd, the bytes
 * shuffled. Unwritten chunks hold zero. Its attributes say the core rank of
 * the image, so that it reads back as it was written.
 *
 * @param descriptor What it holds.
 * @param chunk_extents The extents of a chunk.
 * @return nlohmann::json The document of zarr.json.
 * @throws unsupported_operation_error If ZARR has no type of the values.
 */
nlohmann::json make_array_metadata(
	const image_descriptor &descriptor,
	const std::vector<std::size_t> &chunk_extents
);

/**
 * @brief Make the metadata of a group of version 3 that is an image of
 * OME-Zarr 0.5, of one level, the array "0", at a scale of one.
 *
 * @param descriptor What the image holds, which is_ome_image accepts.
 * @return nlohmann::json The document of zarr.json.
 */
nlohmann::json make_ome_group_metadata(const image_descriptor &descriptor);

} // namespace zarr
} // namespace vitrio
