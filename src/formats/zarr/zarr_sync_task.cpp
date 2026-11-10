// SPDX-License-Identifier: LGPL-2.1-or-later

#include "zarr_sync_task.hpp"

#include "zarr_chunk_store.hpp"

#include <utility>

namespace vitrio
{
namespace zarr
{

zarr_sync_task::zarr_sync_task(std::string path)
	: m_path(std::move(path))
{
}

void zarr_sync_task::run()
{
	zarr_chunk_store::sync(m_path);
}

} // namespace zarr
} // namespace vitrio
