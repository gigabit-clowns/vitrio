// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_file_read_format.hpp"

#include "zarr_array_layout.hpp"
#include "zarr_core_rank.hpp"
#include "zarr_image_reader.hpp"
#include "zarr_metadata.hpp"
#include "zarr_multiscale.hpp"
#include "zarr_node.hpp"

#include <formats/image_file_format_registration_macros.hpp>
#include <file_transfer_pool_implementation.hpp>
#include <system/asynchronous_file_transfer_port_opener.hpp>

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/file_transfer_pool.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>

#include <logger.hpp>
#include <tuning.hpp>

#include <boost/filesystem/path.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace vitrio
{
namespace zarr
{

namespace
{

// The path without the separators that end it, whose name is then that of
// the directory it names.
std::string strip_separators(std::string path)
{
	while (path.size() > 1 && (path.back() == '/' || path.back() == '\\'))
	{
		path.pop_back();
	}

	return path;
}

// An image of OME-Zarr, and which of its levels an array is.
struct image_level
{
	zarr_multiscale image;
	std::size_t level;
};

// The image of OME-Zarr of the group above an array, when it lists the
// array among its levels. Metadata of that group that can not be read
// leaves the array without it, which is logged: the array itself still
// reads.
boost::optional<image_level> find_parent_level(const std::string &directory)
{
	const boost::filesystem::path path(strip_separators(directory));
	const auto parent = path.parent_path().string();
	const auto name = path.filename().string();
	if (parent.empty() || !has_zarr_metadata(parent))
	{
		return boost::none;
	}

	try
	{
		const auto node = read_zarr_node(parent);
		if (node.kind != zarr_node_kind::group)
		{
			return boost::none;
		}

		auto image = parse_multiscale(
			node.attributes, node.metadata_path + ": zarr_file_read_format");
		if (!image)
		{
			return boost::none;
		}

		const auto listed = std::find_if(
			image->datasets.cbegin(),
			image->datasets.cend(),
			[&name] (const zarr_dataset &dataset)
			{
				return strip_separators(dataset.path) == name;
			}
		);
		if (listed != image->datasets.cend())
		{
			const auto level = static_cast<std::size_t>(
				listed - image->datasets.cbegin());
			return image_level{std::move(*image), level};
		}
	}
	catch (const std::exception &error)
	{
		VITRIO_LOG_WARN(
			"{}: zarr_file_read_format: The group above the array is passed "
			"over: {}",
			directory,
			error.what()
		);
	}

	return boost::none;
}

// The array a path names, with what it takes to read it and what its
// image of OME-Zarr, if it is a level of one, states of it.
struct located_array
{
	std::string directory;
	std::shared_ptr<const zarr_array_layout> layout;
	image_descriptor descriptor;
	boost::optional<image_level> level;
};

// A path names an array, or a group that is an image of OME-Zarr, of which
// it names the finest level. Opening a file and reading its metadata both
// start here, so that what is read is stated of the array that is opened,
// along the axes it is opened with.
located_array locate_array(const std::string &path)
{
	auto directory = strip_separators(path);
	auto node = read_zarr_node(directory);
	boost::optional<image_level> level;

	if (node.kind == zarr_node_kind::group)
	{
		auto image = parse_multiscale(
			node.attributes, node.metadata_path + ": zarr_file_read_format");
		if (!image)
		{
			throw unsupported_operation_error(
				directory + ": zarr_file_read_format: The group is no image "
				"of OME-Zarr. Name one of its arrays instead."
			);
		}

		level = image_level{std::move(*image), 0};
		directory =
			join_zarr_path(directory, level->image.datasets.front().path);
		node = read_zarr_node(directory);
		if (node.kind != zarr_node_kind::array)
		{
			throw image_file_format_error(
				directory + ": zarr_file_read_format: The finest level of the "
				"image is no array."
			);
		}
	}
	else if (node.kind == zarr_node_kind::array)
	{
		level = find_parent_level(directory);
	}
	else
	{
		throw image_file_error(
			directory + ": zarr_file_read_format: The directory holds no "
			"metadata of ZARR."
		);
	}

	auto layout = std::make_shared<const zarr_array_layout>(node);
	const auto shape = layout->get_shape();
	if (shape.empty())
	{
		throw unsupported_operation_error(
			directory + ": zarr_file_read_format: Arrays of no axes are not "
			"read."
		);
	}

	const auto core_rank = decide_core_rank(
		shape.size(),
		node.attributes,
		layout->get_dimension_names(),
		level ? &level->image : nullptr
	);

	image_descriptor descriptor(
		shape, core_rank, layout->get_chunk_layout().get_data_type());

	return {
		std::move(directory),
		std::move(layout),
		std::move(descriptor),
		std::move(level)
	};
}

} // anonymous namespace

zarr_file_read_format::zarr_file_read_format()
	: m_buffers(std::make_shared<zarr_chunk_buffers>(get_chunk_byte_budget()))
{
}

std::string zarr_file_read_format::get_name() const
{
	return "ZARR";
}

image_file_format_suitability
zarr_file_read_format::get_suitability(const image_file_probe &probe) const
{
	if (probe.is_directory() && has_zarr_metadata(probe.get_path()))
	{
		return image_file_format_suitability::normal;
	}

	return image_file_format_suitability::unsupported;
}

std::shared_ptr<image_reader> zarr_file_read_format::open(
	const image_file_probe &probe,
	const file_transfer_pool &transfers
) const
{
	auto located = locate_array(probe.get_path());

	return std::make_shared<zarr_image_reader>(
		std::move(located.directory),
		std::move(located.layout),
		std::move(located.descriptor),
		transfers.get_implementation().get_executor(),
		m_buffers,
		asynchronous_file_transfer_port_opener::is_supported()
	);
}

image_metadata
zarr_file_read_format::read_metadata(const image_file_probe &probe) const
{
	const auto located = locate_array(probe.get_path());

	// An array that is no level of an image of OME-Zarr states nothing
	// that is read.
	if (!located.level)
	{
		return image_metadata();
	}

	return derive_metadata(
		located.level->image,
		located.level->level,
		located.descriptor
	);
}

VITRIO_REGISTER_IMAGE_FILE_READ_FORMAT(
	zarr,
	vitrio::zarr::zarr_file_read_format
);

} // namespace zarr
} // namespace vitrio
