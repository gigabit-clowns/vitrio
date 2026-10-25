// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace vitrio
{
namespace zarr
{

/**
 * @brief What a directory of a ZARR hierarchy is.
 */
enum class zarr_node_kind
{
	none,
	array,
	group
};

/**
 * @brief A node of a ZARR hierarchy, an array or a group, as its metadata
 * documents state it.
 *
 * Version 2 keeps the metadata of an array in .zarray, that of a group in
 * .zgroup and the attributes of either in .zattrs. Version 3 keeps all of it
 * in zarr.json, the attributes under "attributes".
 */
struct zarr_node
{
	/** @brief Whether the directory is an array, a group or neither. */
	zarr_node_kind kind = zarr_node_kind::none;

	/** @brief The version of ZARR, 2 or 3, and 0 for no node. */
	int version = 0;

	/** @brief The document that holds the metadata. */
	std::string metadata_path;

	/** @brief The metadata, attributes left out. */
	nlohmann::json metadata;

	/** @brief The attributes, an empty object when there are none. */
	nlohmann::json attributes = nlohmann::json::object();
};

/**
 * @brief Tell whether a directory holds a document of ZARR metadata, without
 * reading it.
 *
 * @param directory The directory.
 * @return true It holds zarr.json, .zarray or .zgroup.
 * @return false It holds none of them.
 */
bool has_zarr_metadata(const std::string &directory) noexcept;

/**
 * @brief Read the node a directory is.
 *
 * A directory holding zarr.json is a node of version 3, whatever else it
 * holds. Otherwise .zarray makes it an array of version 2 and .zgroup a
 * group.
 *
 * @param directory The directory.
 * @return zarr_node The node, of kind none when the directory is none.
 * @throws image_file_error If a document can not be read.
 * @throws image_file_format_error If a document is not JSON, is not an
 * object, or states another version or kind of node than its name does.
 */
zarr_node read_zarr_node(const std::string &directory);

/**
 * @brief Join a path and a name of an entry in it, with the separator of the
 * system.
 *
 * @param directory The path.
 * @param name The name, which may hold separators of its own.
 * @return std::string The joined path.
 */
std::string join_zarr_path(
	const std::string &directory,
	const std::string &name
);

} // namespace zarr
} // namespace vitrio
