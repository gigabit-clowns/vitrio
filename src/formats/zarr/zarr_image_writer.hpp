// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_chunk_store.hpp"

#include <file_transfer/file_transfer_executor.hpp>

#include <vitrio/array/const_array.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_transfer_plan.hpp>
#include <vitrio/image_writer.hpp>

#include <memory>

namespace vitrio
{
namespace zarr
{

/**
 * @brief A writer of one ZARR array, through the executor of a pool.
 *
 * A write is resolved into the chunks it reaches on the thread that starts
 * it, and carried out by the executor, a task a chunk, as
 * zarr_write_operation does: the chunks are held in memory as
 * zarr_chunk_store says, and stored once they are whole. A flush stores those
 * that are not and syncs every file written since the last.
 *
 * A writer that is destroyed flushes itself as @ref flush_async does,
 * without waiting, and a failure of it can only be logged. A writer whose
 * failures matter is flushed beforehand.
 */
class zarr_image_writer final
	: public image_writer
{
public:
	/**
	 * @brief Open a writer over an array whose metadata has been written.
	 *
	 * @param descriptor What the array holds.
	 * @param store Its chunks. Must not be null.
	 * @param transfers What carries out the writes. Must not be null.
	 */
	zarr_image_writer(
		image_descriptor descriptor,
		std::shared_ptr<zarr_chunk_store> store,
		std::shared_ptr<file_transfer_executor> transfers
	);

	~zarr_image_writer() override;

	const image_descriptor& get_descriptor() const noexcept override;

	void write_async(
		const_array source,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) override;

	void flush_async(std::shared_ptr<completion_notifier> done) override;

private:
	image_descriptor m_descriptor;
	std::shared_ptr<zarr_chunk_store> m_store;
	std::shared_ptr<file_transfer_executor> m_transfers;
};

} // namespace zarr
} // namespace vitrio
