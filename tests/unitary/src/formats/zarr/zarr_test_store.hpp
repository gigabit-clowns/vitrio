// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "../../fixtures/scoped_directory.hpp"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <fstream>
#include <string>

namespace vitrio
{

// Writes a file of a store, its directories included.
inline void write_store_file(
	const std::string &directory,
	const std::string &name,
	const std::string &contents
)
{
	const auto path = boost::filesystem::path(directory) / name;
	boost::filesystem::create_directories(path.parent_path());
	std::ofstream output(
		path.string().c_str(), std::ios::out | std::ios::binary);
	output << contents;
}

// Writes the metadata of an array of version 3 of bytes, of 4 by 4 in chunks
// of 2 by 2 stored as they are, whose fill value is 7.
inline void write_byte_array_metadata(const std::string &directory)
{
	write_store_file(directory, "zarr.json",
		R"({"zarr_format": 3, "node_type": "array", "shape": [4, 4],
			"data_type": "uint8",
			"chunk_grid": {"name": "regular",
				"configuration": {"chunk_shape": [2, 2]}},
			"chunk_key_encoding": {"name": "default"},
			"fill_value": 7,
			"codecs": [{"name": "bytes"}]})");
}

} // namespace vitrio
