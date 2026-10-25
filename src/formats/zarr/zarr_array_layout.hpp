// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_chunk_key_encoding.hpp"
#include "zarr_chunk_layout.hpp"
#include "zarr_codec_pipeline.hpp"
#include "zarr_node.hpp"

#include <vitrio/byte.hpp>
#include <vitrio/span.hpp>

#include <boost/optional.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief How a ZARR array lays its values out in its chunks and names their
 * files, as its metadata states it, in version 2 or 3.
 *
 * Only what reads a regular grid of chunks unsharded is accepted: version 2
 * without filters, and version 3 with transposes, then the bytes codec, then
 * codecs that turn bytes into bytes. The codecs of either version are those
 * of zarr_codec_pipeline.
 */
class zarr_array_layout
{
public:
	/**
	 * @brief Read the layout of an array from its node.
	 *
	 * @param node The node, which must be an array.
	 * @throws std::invalid_argument If @p node is not an array.
	 * @throws image_file_format_error If the metadata is not what ZARR
	 * states. The message starts with the path of its document.
	 * @throws unsupported_operation_error If it states what ZARR allows but
	 * this reader does not read, such as a codec it lacks, sharding, a
	 * filter or an extension it does not know.
	 */
	explicit zarr_array_layout(const zarr_node &node);

	/**
	 * @brief Get the extents of the array.
	 *
	 * @return span<const std::size_t> The extents, slowest axis first. It
	 * refers to storage owned by this layout.
	 */
	span<const std::size_t> get_shape() const noexcept;

	/**
	 * @brief Get how the values lie in a decoded chunk.
	 *
	 * @return const zarr_chunk_layout& The layout of a chunk.
	 */
	const zarr_chunk_layout& get_chunk_layout() const noexcept;

	/**
	 * @brief Get what a chunk that is not stored holds, one element of it.
	 *
	 * @return span<const byte> The bytes of one element, in the byte order
	 * of the system. It refers to storage owned by this layout.
	 */
	span<const byte> get_fill_element() const noexcept;

	/**
	 * @brief Get the codecs a chunk is encoded with.
	 *
	 * @return const zarr_codec_pipeline& The codecs.
	 */
	const zarr_codec_pipeline& get_codecs() const noexcept;

	/**
	 * @brief Get how the file of each chunk is named.
	 *
	 * @return const zarr_chunk_key_encoding& The encoding.
	 */
	const zarr_chunk_key_encoding& get_key_encoding() const noexcept;

	/**
	 * @brief Get the name of each axis, as version 3 may state them.
	 *
	 * @return const std::vector<std::string>& One name per axis, empty for
	 * an axis that has none, or no names at all when none are stated.
	 */
	const std::vector<std::string>& get_dimension_names() const noexcept;

private:
	void read_v2(const zarr_node &node, const std::string &where);
	void read_v3(const zarr_node &node, const std::string &where);

	std::vector<std::size_t> m_shape;
	boost::optional<zarr_chunk_layout> m_chunk;
	std::vector<byte> m_fill;
	zarr_codec_pipeline m_codecs;
	zarr_chunk_key_encoding m_keys;
	std::vector<std::string> m_dimension_names;
};

} // namespace zarr
} // namespace vitrio
