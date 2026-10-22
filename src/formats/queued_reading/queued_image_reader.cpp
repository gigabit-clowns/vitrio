// SPDX-License-Identifier: LGPL-2.1-or-later

#include "queued_image_reader.hpp"

#include "image_queued_read.hpp"

#include <formats/read_planning/image_read_policy.hpp>

#include <vitrio/array/array_ref.hpp>
#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/interprocess/exceptions.hpp>

#include <assert.hpp>

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
			boost::interprocess::read_only
		);
	}
	catch (const boost::interprocess::interprocess_exception &error)
	{
		throw image_file_error(
			path + ": queued_image_reader: The file could not be opened: " +
			error.what()
		);
	}
}

std::size_t measure_file(const std::string &path)
{
	boost::system::error_code error;
	const auto size = boost::filesystem::file_size(path, error);
	if (error)
	{
		throw image_file_error(
			path + ": queued_image_reader: The size of the file could not "
			"be read: " + error.message()
		);
	}

	return static_cast<std::size_t>(size);
}

} // anonymous namespace

queued_image_reader::queued_image_reader(
	const std::string &path,
	raw_image_layout layout,
	image_metadata metadata,
	std::shared_ptr<file_read_queue_pool> queues
)
	: m_path(path)
	, m_file(open_file(path))
	, m_file_size(measure_file(path))
	, m_layout(std::move(layout))
	, m_metadata(std::move(metadata))
	, m_queues(std::move(queues))
{
	VITRIO_ASSERT(m_queues != nullptr);

	const auto required =
		m_layout.get_data_offset() + m_layout.get_data_size();
	if (m_file_size < required)
	{
		throw image_file_format_error(
			path + ": queued_image_reader: The file is shorter than the "
			"values it is said to hold."
		);
	}
}

const image_descriptor& queued_image_reader::get_descriptor() const noexcept
{
	return m_layout.get_descriptor();
}

const image_metadata& queued_image_reader::get_metadata() const noexcept
{
	return m_metadata;
}

void queued_image_reader::read(
	array_ref destination,
	const image_transfer_plan &regions
) const
{
	read_queued_regions(
		m_path,
		m_file.get_mapping_handle().handle,
		m_file_size,
		m_layout,
		destination,
		regions,
		m_queues->get_queue(),
		default_staging_budget
	);
}

} // namespace vitrio
