// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_node.hpp"

#include "zarr_json.hpp"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

namespace vitrio
{
namespace zarr
{

namespace
{

const char *const v3_document = "zarr.json";
const char *const v2_array_document = ".zarray";
const char *const v2_group_document = ".zgroup";
const char *const v2_attributes_document = ".zattrs";

bool is_file(const std::string &path) noexcept
{
	boost::system::error_code error;
	return boost::filesystem::is_regular_file(path, error);
}

void check_format(
	const nlohmann::json &metadata,
	int version,
	const std::string &where
)
{
	const auto &format = get_member(metadata, "zarr_format", where);
	if (!format.is_number_integer() || format.get<int>() != version)
	{
		reject_metadata(
			where,
			"The metadata is not of version " + std::to_string(version) +
				" of ZARR."
		);
	}
}

zarr_node read_v3_node(const std::string &path)
{
	const auto where = path + ": read_zarr_node";

	zarr_node node;
	node.version = 3;
	node.metadata_path = path;
	node.metadata = read_json_document(path);
	check_format(node.metadata, 3, where);

	const auto &type = get_member(node.metadata, "node_type", where);
	if (type == "array")
	{
		node.kind = zarr_node_kind::array;
	}
	else if (type == "group")
	{
		node.kind = zarr_node_kind::group;
	}
	else
	{
		reject_metadata(where, "The node is neither an array nor a group.");
	}

	const auto attributes = node.metadata.find("attributes");
	if (attributes != node.metadata.end())
	{
		if (!attributes->is_object())
		{
			reject_metadata(where, "The attributes are not a JSON object.");
		}
		node.attributes = *attributes;
		node.metadata.erase(attributes);
	}

	return node;
}

zarr_node read_v2_node(
	const std::string &directory,
	const std::string &path,
	zarr_node_kind kind
)
{
	zarr_node node;
	node.kind = kind;
	node.version = 2;
	node.metadata_path = path;
	node.metadata = read_json_document(path);
	check_format(node.metadata, 2, path + ": read_zarr_node");

	const auto attributes_path =
		join_zarr_path(directory, v2_attributes_document);
	if (is_file(attributes_path))
	{
		node.attributes = read_json_document(attributes_path);
		if (!node.attributes.is_object())
		{
			reject_metadata(
				attributes_path + ": read_zarr_node",
				"The attributes are not a JSON object."
			);
		}
	}

	return node;
}

} // anonymous namespace

bool has_zarr_metadata(const std::string &directory) noexcept
{
	return
		is_file(join_zarr_path(directory, v3_document)) ||
		is_file(join_zarr_path(directory, v2_array_document)) ||
		is_file(join_zarr_path(directory, v2_group_document));
}

zarr_node read_zarr_node(const std::string &directory)
{
	const auto v3 = join_zarr_path(directory, v3_document);
	if (is_file(v3))
	{
		return read_v3_node(v3);
	}

	const auto array = join_zarr_path(directory, v2_array_document);
	if (is_file(array))
	{
		return read_v2_node(directory, array, zarr_node_kind::array);
	}

	const auto group = join_zarr_path(directory, v2_group_document);
	if (is_file(group))
	{
		return read_v2_node(directory, group, zarr_node_kind::group);
	}

	return zarr_node();
}

std::string join_zarr_path(
	const std::string &directory,
	const std::string &name
)
{
	return (boost::filesystem::path(directory) / name).string();
}

} // namespace zarr
} // namespace vitrio
