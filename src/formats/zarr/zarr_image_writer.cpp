// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_image_writer.hpp"

#include "zarr_flush_operation.hpp"
#include "zarr_write_operation.hpp"

#include <array/array_data.hpp>
#include <strided_transfer/image_chunk_regions.hpp>
#include <strided_transfer/image_region_bounds.hpp>

#include <vitrio/array/const_array_ref.hpp>

#include <assert.hpp>
#include <logger.hpp>

#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

namespace vitrio
{
namespace zarr
{

namespace
{

// Logs what failed the flush of a writer that is gone, which nothing else
// can be told of.
class closing_flush_notifier final
	: public completion_notifier
{
public:
	explicit closing_flush_notifier(std::string path)
		: m_path(std::move(path))
	{
	}

	void notify_done(std::exception_ptr error) noexcept override
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
				"{}: Failed to flush a ZARR array while closing it: {}",
				m_path,
				failure.what()
			);
		}
		catch (...)
		{
			VITRIO_LOG_ERROR(
				"{}: Failed to flush a ZARR array while closing it.",
				m_path
			);
		}
	}

private:
	std::string m_path;
};

} // anonymous namespace

zarr_image_writer::zarr_image_writer(
	image_descriptor descriptor,
	std::shared_ptr<zarr_chunk_store> store,
	std::shared_ptr<file_transfer_executor> transfers
)
	: m_descriptor(std::move(descriptor))
	, m_store(std::move(store))
	, m_transfers(std::move(transfers))
{
	VITRIO_ASSERT(m_store != nullptr);
	VITRIO_ASSERT(m_transfers != nullptr);
}

zarr_image_writer::~zarr_image_writer()
{
	try
	{
		m_transfers->submit(
			std::make_unique<zarr_flush_operation>(m_store),
			std::make_shared<closing_flush_notifier>(m_store->get_directory())
		);
	}
	catch (...)
	{
		VITRIO_LOG_ERROR(
			"{}: Failed to start the flush of a ZARR array while closing it.",
			m_store->get_directory()
		);
	}
}

const image_descriptor& zarr_image_writer::get_descriptor() const noexcept
{
	return m_descriptor;
}

void zarr_image_writer::write_async(
	const_array source,
	image_transfer_plan regions,
	std::shared_ptr<completion_notifier> done
)
{
	if (!done)
	{
		throw std::invalid_argument(
			m_store->get_directory() + ": zarr_image_writer: The notifier "
			"must not be null."
		);
	}

	std::unique_ptr<file_transfer_operation> operation;
	try
	{
		// Refuses an array that is not initialized.
		get_array_data(const_array_ref(source));

		const auto &array = source.get_descriptor();
		check_region_bounds(
			regions,
			m_descriptor,
			array.get_extents(),
			array.get_strides(),
			array.get_offset()
		);

		const auto extents = m_descriptor.get_extents();
		image_chunk_regions chunks(
			regions,
			extents,
			m_store->get_layout().get_chunk_layout().get_extents()
		);
		if (chunks.get_chunk_count() == 0)
		{
			done->notify_done();
			return;
		}

		operation = std::make_unique<zarr_write_operation>(
			m_store, std::move(source), std::move(chunks));
	}
	catch (...)
	{
		done->notify_done(std::current_exception());
		return;
	}

	m_transfers->submit(std::move(operation), std::move(done));
}

void zarr_image_writer::flush_async(std::shared_ptr<completion_notifier> done)
{
	if (!done)
	{
		throw std::invalid_argument(
			m_store->get_directory() + ": zarr_image_writer: The notifier "
			"must not be null."
		);
	}

	std::unique_ptr<file_transfer_operation> operation;
	try
	{
		operation = std::make_unique<zarr_flush_operation>(m_store);
	}
	catch (...)
	{
		done->notify_done(std::current_exception());
		return;
	}

	m_transfers->submit(std::move(operation), std::move(done));
}

} // namespace zarr
} // namespace vitrio
