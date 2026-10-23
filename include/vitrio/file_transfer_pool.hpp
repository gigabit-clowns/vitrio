// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/concurrency/executor.hpp>
#include <vitrio/export.hpp>

#include <memory>

namespace vitrio
{

/**
 * @brief Threads that carry out the reads and writes of files, each many at
 * once, and hand the computations those need to an executor.
 *
 * A file format is given a pool when it opens a file, and the reader or writer
 * it opens keeps what it needs of the pool, so the pool may go before it.
 *
 * The computations never wait. Work that waits for transfers of this pool
 * must not occupy every thread of the executor, or the transfers could not
 * finish.
 *
 * The threads live until the pool and every reader and writer opened with it
 * are gone, and the last of them waits for the transfers under way as it
 * goes. None of them may go on a thread of the executor while a transfer is
 * under way.
 */
class file_transfer_pool final
{
public:
	/**
	 * @brief What the library carries out the transfers with, which the
	 * public headers do not define.
	 */
	class implementation;

	/**
	 * @brief Start the threads.
	 *
	 * @param tasks Where the computations the transfers need run. Must not be
	 * null.
	 * @throws std::invalid_argument If @p tasks is null.
	 * @throws std::system_error If a thread can not be started.
	 */
	VITRIO_API
	explicit file_transfer_pool(std::shared_ptr<executor> tasks);

	file_transfer_pool(const file_transfer_pool &other) = delete;
	file_transfer_pool(file_transfer_pool &&other) = delete;

	VITRIO_API
	~file_transfer_pool();

	file_transfer_pool& operator=(const file_transfer_pool &other) = delete;
	file_transfer_pool& operator=(file_transfer_pool &&other) = delete;

	/**
	 * @brief Get what the library carries out the transfers with.
	 *
	 * It is for the library alone, which does not export it.
	 *
	 * @return const implementation& What the pool holds.
	 */
	const implementation& get_implementation() const noexcept;

private:
	std::unique_ptr<implementation> m_implementation;
};

} // namespace vitrio
