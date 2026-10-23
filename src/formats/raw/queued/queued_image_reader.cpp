// SPDX-License-Identifier: LGPL-2.1-or-later

#include "queued_image_reader.hpp"

#include "make_image_read_operation.hpp"

#include <formats/raw/planning/image_read_policy.hpp>

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/image_file_format_error.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/interprocess/exceptions.hpp>

#include <assert.hpp>

#include <exception>
#include <stdexcept>
#include <utility>

namespace vitrio
{

namespace
{

std::shared_ptr<const boost::interprocess::file_mapping>
open_file(const std::string &path)
{
	try
	{
		return std::make_shared<const boost::interprocess::file_mapping>(
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
	std::shared_ptr<file_transfer_executor> transfers
)
	: m_path(path)
	, m_file(open_file(path))
	, m_file_size(measure_file(path))
	, m_layout(std::move(layout))
	, m_metadata(std::move(metadata))
	, m_transfers(std::move(transfers))
{
	VITRIO_ASSERT(m_transfers != nullptr);

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

queued_image_reader::~queued_image_reader() = default;

const image_descriptor& queued_image_reader::get_descriptor() const noexcept
{
	return m_layout.get_descriptor();
}

const image_metadata& queued_image_reader::get_metadata() const noexcept
{
	return m_metadata;
}

void queued_image_reader::read_async(
	array destination,
	image_transfer_plan regions,
	std::shared_ptr<completion_notifier> done
) const
{
	if (!done)
	{
		throw std::invalid_argument(
			m_path + ": queued_image_reader: The notifier must not be null."
		);
	}

	std::unique_ptr<file_transfer_operation> operation;
	try
	{
		operation = make_image_read_operation(
			m_path,
			m_file,
			m_file_size,
			m_layout,
			std::move(destination),
			regions,
			default_staging_budget
		);
	}
	catch (...)
	{
		done->notify_done(std::current_exception());
		return;
	}

	m_transfers->submit(std::move(operation), std::move(done));
}

} // namespace vitrio
