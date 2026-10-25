// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_image_reader.hpp"

#include "zarr_read_operation.hpp"

#include <array/array_data.hpp>
#include <strided_transfer/image_chunk_regions.hpp>
#include <strided_transfer/image_region_bounds.hpp>

#include <vitrio/array/array_ref.hpp>

#include <assert.hpp>
#include <tuning.hpp>

#include <stdexcept>
#include <utility>

namespace vitrio
{
namespace zarr
{

zarr_image_reader::zarr_image_reader(
	std::string directory,
	std::shared_ptr<const zarr_array_layout> layout,
	image_descriptor descriptor,
	image_metadata metadata,
	std::shared_ptr<file_transfer_executor> transfers,
	bool asynchronous
)
	: m_directory(std::move(directory))
	, m_layout(std::move(layout))
	, m_descriptor(std::move(descriptor))
	, m_metadata(std::move(metadata))
	, m_transfers(std::move(transfers))
	, m_asynchronous(asynchronous)
{
	VITRIO_ASSERT(m_layout != nullptr);
	VITRIO_ASSERT(m_transfers != nullptr);
}

zarr_image_reader::~zarr_image_reader() = default;

const image_descriptor& zarr_image_reader::get_descriptor() const noexcept
{
	return m_descriptor;
}

const image_metadata& zarr_image_reader::get_metadata() const noexcept
{
	return m_metadata;
}

void zarr_image_reader::read_async(
	array destination,
	image_transfer_plan regions,
	std::shared_ptr<completion_notifier> done
) const
{
	if (!done)
	{
		throw std::invalid_argument(
			m_directory + ": zarr_image_reader: The notifier must not be null."
		);
	}

	std::unique_ptr<file_transfer_operation> operation;
	try
	{
		// Refuses an array that is not initialized.
		get_array_data(array_ref(destination));

		const auto &target = destination.get_descriptor();
		check_region_bounds(
			regions,
			m_descriptor,
			target.get_extents(),
			target.get_strides(),
			target.get_offset()
		);

		image_chunk_regions chunks(
			regions,
			m_layout->get_shape(),
			m_layout->get_chunk_layout().get_extents()
		);
		if (chunks.get_chunk_count() == 0)
		{
			done->notify_done();
			return;
		}

		operation = std::make_unique<zarr_read_operation>(
			m_directory,
			m_layout,
			std::move(destination),
			std::move(chunks),
			m_asynchronous,
			get_chunk_byte_budget(),
			get_chunks_in_flight()
		);
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
