// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/export.hpp>

#include <cstddef>
#include <memory>

namespace vitrio
{

class image_scratch_storage;

/**
 * @brief Create storage for a scratch in main memory.
 *
 * The memory is aligned to the page size of the system, and is released
 * when the storage is destroyed. Its contents are not defined at first.
 *
 * @param size Size of the storage in bytes.
 * @return std::shared_ptr<image_scratch_storage> The storage, of @p size
 * bytes. Never null.
 * @throws std::invalid_argument If @p size is zero.
 * @throws std::bad_alloc If the memory can not be allocated.
 */
VITRIO_API
std::shared_ptr<image_scratch_storage>
create_host_image_scratch_storage(std::size_t size);

} // namespace vitrio
