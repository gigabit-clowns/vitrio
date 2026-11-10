// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/concurrency/task.hpp>

#include <string>

namespace vitrio
{
namespace zarr
{

/**
 * @brief A task that makes what was written to one file of a ZARR store
 * reach the storage.
 *
 * A write of a store runs one for each file of a batch of chunks, and a
 * flush for each file left, so that the files of a batch are synced at
 * once.
 */
class zarr_sync_task final
	: public task
{
public:
	/**
	 * @brief Prepare the sync of a file.
	 *
	 * @param path The file. One that is gone by the time the task runs, as
	 * a chunk of the fill value stored since, is passed over.
	 */
	explicit zarr_sync_task(std::string path);

	/**
	 * @throws image_file_error If the file can not be synced.
	 */
	void run() override;

private:
	std::string m_path;
};

} // namespace zarr
} // namespace vitrio
