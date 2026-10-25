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

} // namespace vitrio
