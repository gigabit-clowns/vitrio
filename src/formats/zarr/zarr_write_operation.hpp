// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "zarr_chunk_store.hpp"

#include <file_transfer/file_transfer_operation.hpp>
#include <strided_transfer/image_chunk_regions.hpp>

#include <vitrio/array/const_array.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace vitrio
{
namespace zarr
{

/**
 * @brief A write of regions of an array into a ZARR array, a chunk at a
 * time.
 *
 * Each chunk the regions reach is written by a task of its own on the
 * executor of computations, which converts the pieces of the regions on it
 * into it, and encodes and writes it once it is whole. The tasks of chunks
 * apart are carried out at once.
 */
class zarr_write_operation final
	: public file_transfer_operation
{
public:
	/**
	 * @brief Prepare a write.
	 *
	 * @param store The chunks of the array.
	 * @param source The values to write. It is kept until the write is done.
	 * @param chunks The regions, resolved into the chunks they reach. They
	 * must lie in the array and in @p source.
	 */
	zarr_write_operation(
		std::shared_ptr<zarr_chunk_store> store,
		const_array source,
		image_chunk_regions chunks
	);

	~zarr_write_operation() override;

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
	class chunk_write;

	void write(std::size_t position) const;

	std::shared_ptr<zarr_chunk_store> m_store;
	const_array m_source;
	image_chunk_regions m_chunks;
};

} // namespace zarr
} // namespace vitrio
