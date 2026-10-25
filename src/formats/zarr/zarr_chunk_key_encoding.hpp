// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/span.hpp>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>

namespace vitrio
{
namespace zarr
{

/**
 * @brief How a ZARR array names the file of each chunk.
 *
 * The name of a chunk is its position along each axis, joined by a
 * separator, behind a prefix: "c/1/0/2" in the default encoding of version
 * 3, and "1.0.2" or "1/0/2" in version 2, which has no prefix. A separator of
 * "/" makes each but the last axis a directory.
 */
class zarr_chunk_key_encoding
{
public:
	/**
	 * @brief Construct an encoding from its parts.
	 *
	 * @param prefix What the name of every chunk starts with, nothing for
	 * none. It is followed by the separator unless the array has no axes.
	 * @param separator What joins the positions, '.' or '/'.
	 */
	zarr_chunk_key_encoding(std::string prefix, char separator);

	/**
	 * @brief Get the name of the file of a chunk, relative to the directory
	 * of its array.
	 *
	 * @param chunk The position of the chunk along each axis.
	 * @return std::string The name, which holds '/' where the separator is
	 * one. An array of no axes names its single chunk by the prefix, or "0"
	 * when there is none.
	 */
	std::string get_key(span<const std::size_t> chunk) const;

private:
	std::string m_prefix;
	char m_separator;
};

/**
 * @brief Read how an array of version 2 names its chunks.
 *
 * @param metadata The metadata of .zarray, whose "dimension_separator"
 * is "." when absent.
 * @param where What a message starts with.
 * @return zarr_chunk_key_encoding The encoding.
 * @throws image_file_format_error If the separator is neither "." nor "/".
 */
zarr_chunk_key_encoding parse_v2_chunk_key_encoding(
	const nlohmann::json &metadata,
	const std::string &where
);

/**
 * @brief Read how an array of version 3 names its chunks.
 *
 * @param encoding The value of "chunk_key_encoding": "default", whose
 * separator is "/" unless stated, or "v2", whose separator is ".".
 * @param where What a message starts with.
 * @return zarr_chunk_key_encoding The encoding.
 * @throws image_file_format_error If @p encoding names neither, or states a
 * separator that is neither "." nor "/".
 */
zarr_chunk_key_encoding parse_v3_chunk_key_encoding(
	const nlohmann::json &encoding,
	const std::string &where
);

} // namespace zarr
} // namespace vitrio
