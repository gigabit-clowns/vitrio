// SPDX-License-Identifier: LGPL-2.1-or-later

#include "create_image_file.hpp"

#include <vitrio/exceptions/image_file_error.hpp>

#include <boost/filesystem/operations.hpp>

#include <fstream>

namespace vitrio
{

void create_image_file(const std::string &path, std::size_t size)
{
	if (size == 0)
	{
		throw image_file_error(
			path + ": create_image_file: A file of no bytes holds no image."
		);
	}

	{
		std::filebuf file;
		const auto opened = file.open(
			path,
			std::ios_base::in | std::ios_base::out |
			std::ios_base::trunc | std::ios_base::binary
		);
		if (opened == nullptr)
		{
			throw image_file_error(
				path + ": create_image_file: The file could not be created."
			);
		}
	}

	boost::system::error_code code;
	boost::filesystem::resize_file(path, size, code);
	if (code)
	{
		throw image_file_error(
			path + ": create_image_file: The file could not be sized: " +
			code.message()
		);
	}
}

} // namespace vitrio
