// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace vitrio
{

class file_transfer_executor;
class image_scratch_storage;

/**
 * @brief Create storage for a scratch in a file, as
 * @ref create_file_image_scratch_storage does, given how much its loads
 * stage.
 *
 * A load reads its regions into memory of its own before it writes them, a
 * piece of at most @p piece_bytes at a time, unless a single region is
 * larger. The pieces of the loads of the storage stage @p load_bytes at once
 * at most: a piece that does not fit waits for earlier ones, in the order
 * they came, and one that alone stages more is started alone.
 *
 * @param path Path of the file.
 * @param size Size of the file in bytes. Not zero.
 * @param transfers What carries out the transfers of the file, and starts
 * pieces that waited on its threads of computations.
 * @param piece_bytes The bytes a piece of a load stages at most. Not zero.
 * @param load_bytes The bytes the loads of the storage stage at once at
 * most. Not zero.
 * @return std::shared_ptr<image_scratch_storage> The storage. Never null.
 * @throws image_file_error If @p size is zero, or if the file can not be
 * created, sized, given its room or opened.
 */
std::shared_ptr<image_scratch_storage> create_file_image_scratch_storage(
	const std::string &path,
	std::size_t size,
	std::shared_ptr<file_transfer_executor> transfers,
	std::size_t piece_bytes,
	std::size_t load_bytes
);

} // namespace vitrio
