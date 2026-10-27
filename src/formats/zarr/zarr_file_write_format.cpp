// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_file_write_format.hpp"

#include "zarr_array_layout.hpp"
#include "zarr_chunk_store.hpp"
#include "zarr_image_writer.hpp"
#include "zarr_json.hpp"
#include "zarr_node.hpp"
#include "zarr_store_metadata.hpp"

#include <file_transfer_pool_implementation.hpp>
#include <formats/image_file_format_registration_macros.hpp>

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/file_transfer_pool.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_probe.hpp>

#include <tuning.hpp>

#include <boost/filesystem/operations.hpp>

#include <memory>

namespace vitrio
{
namespace zarr
{

namespace
{

std::string strip_separators(std::string path)
{
	while (path.size() > 1 && (path.back() == '/' || path.back() == '\\'))
	{
		path.pop_back();
	}

	return path;
}

// Clears the path for a new store: what is there is removed if it is a file
// or a node of ZARR, and a directory that holds anything else is refused.
void clear_path(const std::string &path)
{
	namespace filesystem = boost::filesystem;
	boost::system::error_code error;

	const auto status = filesystem::status(path, error);
	if (status.type() == filesystem::file_not_found)
	{
		error.clear();
	}
	else if (filesystem::is_directory(status))
	{
		if (has_zarr_metadata(path))
		{
			filesystem::remove_all(path, error);
		}
		else if (!filesystem::is_empty(path, error) && !error)
		{
			throw image_file_error(
				path + ": zarr_file_write_format: The directory holds what is "
				"no ZARR store, which is not replaced."
			);
		}
	}
	else if (!error)
	{
		filesystem::remove(path, error);
	}
	if (error)
	{
		throw image_file_error(
			path + ": zarr_file_write_format: What is at the path could not "
			"be replaced: " + error.message()
		);
	}

	filesystem::create_directories(path, error);
	if (error)
	{
		throw image_file_error(
			path + ": zarr_file_write_format: The directory could not be "
			"created: " + error.message()
		);
	}
}

} // anonymous namespace

std::string zarr_file_write_format::get_name() const
{
	return "ZARR";
}

image_file_format_suitability
zarr_file_write_format::get_suitability(const image_file_probe &probe) const
{
	if (
		probe.get_extension() == ".zarr" ||
		(probe.is_directory() && has_zarr_metadata(probe.get_path()))
	)
	{
		return image_file_format_suitability::normal;
	}

	return image_file_format_suitability::unsupported;
}

std::shared_ptr<image_writer> zarr_file_write_format::open(
	const image_file_probe &probe,
	const image_descriptor &descriptor,
	const image_metadata &/*metadata*/,
	const file_transfer_pool &transfers
) const
{
	// The metadata is made first, so that what ZARR can not hold is refused
	// before anything at the path is replaced.
	const auto chunks = choose_chunk_extents(
		descriptor, get_zarr_image_chunk_edge(), get_zarr_volume_chunk_edge());
	const auto array_metadata = make_array_metadata(descriptor, chunks);

	const auto root = strip_separators(probe.get_path());
	clear_path(root);

	std::vector<std::string> written;
	auto directory = root;
	if (is_ome_image(descriptor))
	{
		const auto group = join_zarr_path(root, "zarr.json");
		write_json_document(group, make_ome_group_metadata(descriptor));
		written.push_back(group);

		directory = join_zarr_path(root, "0");
		boost::system::error_code error;
		boost::filesystem::create_directories(directory, error);
		if (error)
		{
			throw image_file_error(
				directory + ": zarr_file_write_format: The directory of the "
				"array could not be created: " + error.message()
			);
		}
	}

	const auto array = join_zarr_path(directory, "zarr.json");
	write_json_document(array, array_metadata);
	written.push_back(array);

	// The array is read back as it is to be read, so that what is written
	// is what the reader reads.
	const auto layout = std::make_shared<const zarr_array_layout>(
		read_zarr_node(directory));
	const auto store = std::make_shared<zarr_chunk_store>(
		directory, layout, get_chunk_write_byte_budget());
	for (const auto &path : written)
	{
		store->add_unsynced(path);
	}

	return std::make_shared<zarr_image_writer>(
		descriptor,
		store,
		transfers.get_implementation().get_executor()
	);
}

VITRIO_REGISTER_IMAGE_FILE_WRITE_FORMAT(
	zarr,
	vitrio::zarr::zarr_file_write_format
);

} // namespace zarr
} // namespace vitrio
