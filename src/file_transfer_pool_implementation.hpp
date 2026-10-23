// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/file_transfer_pool.hpp>

#include <file_transfer/file_transfer_executor.hpp>

#include <vitrio/concurrency/executor.hpp>

#include <memory>

namespace vitrio
{

/**
 * @brief What a @ref file_transfer_pool holds: an executor of file transfers,
 * with @c VITRIO_FILE_TRANSFER_WORKERS threads, each with a port that suits
 * the system.
 *
 * The ports are asynchronous where the process can have them, with room for
 * @c VITRIO_FILE_READ_QUEUE_CAPACITY reads each, and read as they are asked
 * otherwise.
 */
class file_transfer_pool::implementation
{
public:
	/**
	 * @brief Start the threads.
	 *
	 * @param tasks Where the computations the transfers need run. Never null.
	 * @throws std::system_error If a thread can not be started.
	 */
	explicit implementation(std::shared_ptr<executor> tasks);

	implementation(const implementation &other) = delete;
	implementation(implementation &&other) = delete;
	~implementation();

	implementation& operator=(const implementation &other) = delete;
	implementation& operator=(implementation &&other) = delete;

	/**
	 * @brief Get the executor that carries out the transfers.
	 *
	 * Whoever keeps a copy keeps the threads.
	 *
	 * @return const std::shared_ptr<file_transfer_executor>& The executor.
	 * Never null.
	 */
	const std::shared_ptr<file_transfer_executor>&
	get_executor() const noexcept;

private:
	std::shared_ptr<file_transfer_executor> m_executor;
};

} // namespace vitrio
