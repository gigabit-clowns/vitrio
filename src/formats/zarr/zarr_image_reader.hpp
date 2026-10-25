// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_array_layout.hpp"

#include <file_transfer/file_transfer_executor.hpp>

#include <vitrio/array/array.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_metadata.hpp>
#include <vitrio/image_reader.hpp>
#include <vitrio/image_transfer_plan.hpp>

#include <memory>
#include <string>

namespace vitrio
{
namespace zarr
{

/**
 * @brief A reader of one ZARR array, through the transfers of a pool.
 *
 * It holds no file open between reads: each read opens the chunks it
 * reaches, as zarr_read_operation says. What it reports is fixed when it is
 * opened, so reads may be under way at once.
 */
class zarr_image_reader final
	: public image_reader
{
public:
	/**
	 * @brief Open a reader over an array whose layout has been read.
	 *
	 * @param directory The directory of the array.
	 * @param layout How it lays out its chunks. Must not be null.
	 * @param descriptor What it holds, its extents those of @p layout.
	 * @param metadata How its samples map onto physical space.
	 * @param transfers What carries out the reads. Must not be null.
	 * @param asynchronous Whether @p transfers goes through asynchronous
	 * ports.
	 */
	zarr_image_reader(
		std::string directory,
		std::shared_ptr<const zarr_array_layout> layout,
		image_descriptor descriptor,
		image_metadata metadata,
		std::shared_ptr<file_transfer_executor> transfers,
		bool asynchronous
	);

	~zarr_image_reader() override;

	const image_descriptor& get_descriptor() const noexcept override;

	const image_metadata& get_metadata() const noexcept override;

	void read_async(
		array destination,
		image_transfer_plan regions,
		std::shared_ptr<completion_notifier> done
	) const override;

private:
	std::string m_directory;
	std::shared_ptr<const zarr_array_layout> m_layout;
	image_descriptor m_descriptor;
	image_metadata m_metadata;
	std::shared_ptr<file_transfer_executor> m_transfers;
	bool m_asynchronous;
};

} // namespace zarr
} // namespace vitrio
