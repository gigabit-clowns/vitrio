// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_chunk_store.hpp"

#include <file_transfer/file_transfer_operation.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vitrio
{
namespace zarr
{

/**
 * @brief A flush of a ZARR array being written: the chunks held in memory
 * stored, then every file written since the last flush synced.
 *
 * Each chunk is stored by a task of its own, and each file synced by
 * another, so that they are under way at once.
 */
class zarr_flush_operation final
	: public file_transfer_operation
{
public:
	/**
	 * @brief Prepare a flush.
	 *
	 * @param store The chunks of the array.
	 */
	explicit zarr_flush_operation(std::shared_ptr<zarr_chunk_store> store);

	~zarr_flush_operation() override;

	void start(file_transfer_scope &scope) override;

	void on_transfer(
		const file_transfer_completion &completion,
		file_transfer_scope &scope
	) override;

	void on_task(
		std::uint64_t tag,
		std::exception_ptr error,
		file_transfer_scope &scope
	) override;

private:
	class step;

	void sync_all(file_transfer_scope &scope);

	std::shared_ptr<zarr_chunk_store> m_store;
	std::vector<std::vector<std::size_t>> m_chunks;
	std::vector<std::string> m_files;
	std::size_t m_storing;
};

} // namespace zarr
} // namespace vitrio
