// SPDX-License-Identifier: LGPL-2.1-or-later

#include "writable_image_file.hpp"

#include <vitrio/exceptions/image_file_error.hpp>

#include <boost/interprocess/exceptions.hpp>

#include <utility>

namespace vitrio
{

namespace
{

boost::interprocess::file_mapping open_file(const std::string &path)
{
	try
	{
		return boost::interprocess::file_mapping(
			path.c_str(),
			boost::interprocess::read_write
		);
	}
	catch (const boost::interprocess::interprocess_exception &error)
	{
		throw image_file_error(
			path + ": writable_image_file: The file could not be opened for "
			"writing: " + error.what()
		);
	}
}

} // anonymous namespace

writable_image_file::writable_image_file(std::string path)
	: m_path(std::move(path))
	, m_file(open_file(m_path))
{
}

writable_image_file::~writable_image_file() = default;

const std::string& writable_image_file::get_path() const noexcept
{
	return m_path;
}

file_handle_t writable_image_file::get_file() const noexcept
{
	return m_file.get_mapping_handle().handle;
}

const boost::interprocess::file_mapping&
writable_image_file::get_open_file() const noexcept
{
	return m_file;
}

} // namespace vitrio
