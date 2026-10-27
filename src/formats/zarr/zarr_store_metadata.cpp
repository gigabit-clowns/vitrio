// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_store_metadata.hpp"

#include <vitrio/exceptions/unsupported_operation_error.hpp>

#include <algorithm>

namespace vitrio
{
namespace zarr
{

namespace
{

const char *const core_names[] = {"z", "y", "x"};

nlohmann::json make_zero(numerical_type type)
{
	switch (type)
	{
	case numerical_type::boolean:
		return false;
	case numerical_type::float16:
	case numerical_type::float32:
	case numerical_type::float64:
		return 0.0;
	case numerical_type::complex_float32:
	case numerical_type::complex_float64:
		return nlohmann::json::array({0.0, 0.0});
	default:
		return 0;
	}
}

} // anonymous namespace

std::string get_v3_data_type_name(numerical_type type)
{
	switch (type)
	{
	case numerical_type::boolean: return "bool";
	case numerical_type::int8: return "int8";
	case numerical_type::uint8: return "uint8";
	case numerical_type::int16: return "int16";
	case numerical_type::uint16: return "uint16";
	case numerical_type::int32: return "int32";
	case numerical_type::uint32: return "uint32";
	case numerical_type::int64: return "int64";
	case numerical_type::uint64: return "uint64";
	case numerical_type::float16: return "float16";
	case numerical_type::float32: return "float32";
	case numerical_type::float64: return "float64";
	case numerical_type::complex_float32: return "complex64";
	case numerical_type::complex_float64: return "complex128";
	default:
		throw unsupported_operation_error(
			"get_v3_data_type_name: ZARR has no type of the values."
		);
	}
}

std::vector<std::size_t> choose_chunk_extents(
	const image_descriptor &descriptor,
	std::size_t image_edge,
	std::size_t volume_edge
)
{
	const auto extents = descriptor.get_extents();
	const auto core = descriptor.get_core_rank();
	const auto edge = core >= 3 ? volume_edge : image_edge;

	std::vector<std::size_t> chunk(extents.size(), 1);
	for (auto axis = extents.size() - core; axis < extents.size(); ++axis)
	{
		chunk[axis] = std::max<std::size_t>(
			1, std::min(std::max<std::size_t>(1, edge), extents[axis]));
	}

	return chunk;
}

std::vector<std::string> name_axes(const image_descriptor &descriptor)
{
	const auto rank = descriptor.get_extents().size();
	const auto core = descriptor.get_core_rank();
	const auto stack = rank - core;

	std::vector<std::string> names;
	for (std::size_t axis = 0; axis < stack; ++axis)
	{
		names.push_back(stack == 1 ? "n" : "n" + std::to_string(axis));
	}
	for (std::size_t axis = 0; axis < core; ++axis)
	{
		// More than three axes of an image have no names of space, and are
		// named as the stack is.
		const auto from_end = core - axis;
		names.push_back(from_end <= 3
			? std::string(core_names[3 - from_end])
			: "a" + std::to_string(axis));
	}

	return names;
}

bool is_ome_image(const image_descriptor &descriptor)
{
	const auto rank = descriptor.get_extents().size();
	const auto core = descriptor.get_core_rank();
	return rank >= 2 && rank <= 5 && core >= 2 && core <= 3;
}

nlohmann::json make_array_metadata(
	const image_descriptor &descriptor,
	const std::vector<std::size_t> &chunk_extents
)
{
	const auto extents = descriptor.get_extents();
	const auto type = descriptor.get_data_type();

	nlohmann::json metadata = {
		{"zarr_format", 3},
		{"node_type", "array"},
		{"shape", std::vector<std::size_t>(extents.begin(), extents.end())},
		{"data_type", get_v3_data_type_name(type)},
		{"chunk_grid", {
			{"name", "regular"},
			{"configuration", {{"chunk_shape", chunk_extents}}},
		}},
		{"chunk_key_encoding", {
			{"name", "default"},
			{"configuration", {{"separator", "/"}}},
		}},
		{"fill_value", make_zero(type)},
		{"codecs", nlohmann::json::array({
			{
				{"name", "bytes"},
				{"configuration", {{"endian", "little"}}},
			},
			{
				{"name", "blosc"},
				{"configuration", {
					{"cname", "zstd"},
					{"clevel", 5},
					{"shuffle", "shuffle"},
					{"typesize", get_size(type)},
					{"blocksize", 0},
				}},
			},
		})},
		{"dimension_names", name_axes(descriptor)},
		{"attributes", {
			{"vitrio", {{"core_rank", descriptor.get_core_rank()}}},
		}},
	};

	return metadata;
}

nlohmann::json make_ome_group_metadata(const image_descriptor &descriptor)
{
	const auto names = name_axes(descriptor);
	const auto rank = names.size();
	const auto core = descriptor.get_core_rank();

	auto axes = nlohmann::json::array();
	for (std::size_t axis = 0; axis < rank; ++axis)
	{
		nlohmann::json entry = {{"name", names[axis]}};
		if (axis >= rank - core)
		{
			entry["type"] = "space";
		}
		axes.push_back(entry);
	}

	return {
		{"zarr_format", 3},
		{"node_type", "group"},
		{"attributes", {
			{"ome", {
				{"version", "0.5"},
				{"multiscales", nlohmann::json::array({{
					{"axes", axes},
					{"datasets", nlohmann::json::array({{
						{"path", "0"},
						{"coordinateTransformations", nlohmann::json::array({{
							{"type", "scale"},
							{"scale", std::vector<double>(rank, 1.0)},
						}})},
					}})},
				}})},
			}},
		}},
	};
}

} // namespace zarr
} // namespace vitrio
