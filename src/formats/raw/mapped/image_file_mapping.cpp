// SPDX-License-Identifier: LGPL-2.1-or-later

#include "image_file_mapping.hpp"

#include <vitrio/exceptions/image_file_error.hpp>

#include <cstddef>

namespace vitrio
{

namespace
{

boost::interprocess::mode_t to_mode(image_file_access access) noexcept
{
	return access == image_file_access::read_write
		? boost::interprocess::read_write
		: boost::interprocess::read_only;
}

[[noreturn]]
void throw_unmappable(
	const std::string &path,
	const boost::interprocess::interprocess_exception &error
)
{
	throw image_file_error(
		path + ": image_file_mapping: The file could not be mapped: " +
		error.what()
	);
}

boost::interprocess::file_mapping open_mapping(
	const std::string &path,
	image_file_access access
)
{
	try
	{
		return boost::interprocess::file_mapping(path.c_str(), to_mode(access));
	}
	catch (const boost::interprocess::interprocess_exception &error)
	{
		throw_unmappable(path, error);
	}
}

boost::interprocess::mapped_region map_region(
	const boost::interprocess::file_mapping &mapping,
	image_file_access access
)
{
	try
	{
		return boost::interprocess::mapped_region(mapping, to_mode(access));
	}
	catch (const boost::interprocess::interprocess_exception &error)
	{
		throw_unmappable(mapping.get_name(), error);
	}
}

} // anonymous namespace

image_file_mapping::image_file_mapping(
	const std::string &path,
	image_file_access access
)
	: m_mapping(open_mapping(path, access))
	, m_region(map_region(m_mapping, access))
{
	if (m_region.get_size() == 0)
	{
		throw image_file_error(
			path + ": image_file_mapping: The file is empty."
		);
	}
}

byte* image_file_mapping::get_data() const noexcept
{
	return static_cast<byte*>(m_region.get_address());
}

std::size_t image_file_mapping::get_size() const noexcept
{
	return m_region.get_size();
}

file_handle_t image_file_mapping::get_file() const noexcept
{
	return m_mapping.get_mapping_handle().handle;
}

} // namespace vitrio
