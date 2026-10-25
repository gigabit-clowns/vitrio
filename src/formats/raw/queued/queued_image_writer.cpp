// SPDX-License-Identifier: LGPL-2.1-or-later

#include "queued_image_writer.hpp"

#include "file_sync_operation.hpp"
#include "make_image_write_operation.hpp"

#include <formats/raw/create_image_file.hpp>
#include <system/file_sync.hpp>
#include <system/scattered_file_write.hpp>

#include <vitrio/const_memory_range.hpp>
#include <vitrio/exceptions/image_file_error.hpp>

#include <assert.hpp>
#include <logger.hpp>
#include <tuning.hpp>

#include <exception>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace vitrio
{

namespace
{

// Sizes the file, writes what precedes its values, and opens it for the
// writes to come.
std::shared_ptr<writable_image_file> lay_out_file(
	const std::string &path,
	const raw_image_layout &layout,
	span<const byte> preamble
)
{
	if (preamble.size() > layout.get_data_offset())
	{
		throw std::invalid_argument(
			"queued_image_writer: The preamble reaches past where the values "
			"of the file begin."
		);
	}

	create_image_file(
		path,
		layout.get_data_offset() + layout.get_data_size()
	);
	auto file = std::make_shared<writable_image_file>(path);

	// A header is small, and is written on the thread that opens the file.
	const const_memory_range buffer(preamble.data(), preamble.size());
	std::error_code error;
	write_scattered(file->get_file(), 0, make_span(&buffer, 1), error);
	if (error)
	{
		throw image_file_error(
			path + ": queued_image_writer: What precedes the values of the "
			"file could not be written: " + error.message()
		);
	}

	return file;
}

// What the sync a writer hands the executor as it is destroyed reports to,
// which can only log a failure: nothing is left to report it to.
class closing_sync_notifier final
	: public completion_notifier
{
public:
	explicit closing_sync_notifier(std::string path) noexcept
		: m_path(std::move(path))
	{
	}

	void notify_done(std::exception_ptr error) override
	{
		if (!error)
		{
			return;
		}

		try
		{
			std::rethrow_exception(error);
		}
		catch (const std::exception &failure)
		{
			VITRIO_LOG_ERROR(
				"Failed to flush an image file while closing it: {}",
				failure.what()
			);
		}
		catch (...)
		{
			VITRIO_LOG_ERROR(
				"{}: Failed to flush an image file while closing it.",
				m_path
			);
		}
	}

private:
	std::string m_path;
};

} // anonymous namespace

queued_image_writer::queued_image_writer(
	const std::string &path,
	raw_image_layout layout,
	span<const byte> preamble,
	std::shared_ptr<file_transfer_executor> transfers
)
	: m_layout(std::move(layout))
	, m_file(lay_out_file(path, m_layout, preamble))
	, m_transfers(std::move(transfers))
{
	VITRIO_ASSERT(m_transfers != nullptr);
}

queued_image_writer::~queued_image_writer()
{
	try
	{
		m_transfers->submit(
			std::make_unique<file_sync_operation>(m_file),
			std::make_shared<closing_sync_notifier>(m_file->get_path())
		);
	}
	catch (...)
	{
		// Short of memory for the operation, the sync is carried out here.
		std::error_code error;
		sync_file(m_file->get_file(), error);
		if (error)
		{
			VITRIO_LOG_ERROR(
				"{}: Failed to flush an image file while closing it: {}",
				m_file->get_path(),
				error.message()
			);
		}
	}
}

const image_descriptor& queued_image_writer::get_descriptor() const noexcept
{
	return m_layout.get_descriptor();
}

void queued_image_writer::write_async(
	const_array source,
	image_transfer_plan regions,
	std::shared_ptr<completion_notifier> done
)
{
	if (!done)
	{
		throw std::invalid_argument(
			m_file->get_path() + ": queued_image_writer: The notifier must "
			"not be null."
		);
	}

	std::unique_ptr<file_transfer_operation> operation;
	try
	{
		operation = make_image_write_operation(
			m_file,
			m_layout,
			std::move(source),
			regions,
			get_staging_byte_budget()
		);
	}
	catch (...)
	{
		done->notify_done(std::current_exception());
		return;
	}

	m_transfers->submit(std::move(operation), std::move(done));
}

void queued_image_writer::flush_async(
	std::shared_ptr<completion_notifier> done
)
{
	if (!done)
	{
		throw std::invalid_argument(
			m_file->get_path() + ": queued_image_writer: The notifier must "
			"not be null."
		);
	}

	std::unique_ptr<file_transfer_operation> operation;
	try
	{
		operation = std::make_unique<file_sync_operation>(m_file);
	}
	catch (...)
	{
		done->notify_done(std::current_exception());
		return;
	}

	m_transfers->submit(std::move(operation), std::move(done));
}

} // namespace vitrio
