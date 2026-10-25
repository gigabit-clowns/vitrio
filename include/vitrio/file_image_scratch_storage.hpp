// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace vitrio
{

class file_transfer_pool;
class image_scratch_storage;

/**
 * @brief Create storage for a scratch in a file.
 *
 * The file is created if it is missing and given the size, and the room it
 * takes on its disk is reserved where its file system can. A file already
 * at @p path is kept: it is not emptied, and its contents stay up to the
 * new size. Storages over the same file share its contents.
 *
 * The contents live in the file, which lets it hold more than the main
 * memory does. Its values are served by reads of the file and taken in by
 * writes of it, which the threads of @p transfers carry out, so a disk that
 * turns out full is a failure of the load that finds it so. What is taken
 * in passes through memory of its own, as much for all the loads of the
 * storage at once as the library is tuned to, whatever the storage holds,
 * and loads beyond that wait their turn. Nothing is synced: the file is a
 * cache, which a scratch checks before it trusts it.
 * The file is not removed when the storage is destroyed.
 *
 * @param path Path of the file.
 * @param size Size of the file in bytes. Not zero.
 * @param transfers The threads that carry out the transfers of the file.
 * @return std::shared_ptr<image_scratch_storage> The storage, of @p size
 * bytes. Never null.
 * @throws image_file_error If @p size is zero, or if the file can not be
 * created, sized, given its room or opened.
 */
VITRIO_API
std::shared_ptr<image_scratch_storage> create_file_image_scratch_storage(
	const std::string &path,
	std::size_t size,
	const file_transfer_pool &transfers
);

} // namespace vitrio
