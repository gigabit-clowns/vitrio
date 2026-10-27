// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <vitrio/concurrency/thread_pool_executor.hpp>
#include <vitrio/file_transfer_pool.hpp>

#include <memory>

namespace vitrio
{
namespace test
{

/**
 * @brief Make a pool of file transfers whose computations run on a thread
 * pool of its own, for the files a case opens.
 *
 * @return std::shared_ptr<const file_transfer_pool> The pool. Never null.
 */
inline std::shared_ptr<const file_transfer_pool> make_file_transfer_pool()
{
	return std::make_shared<const file_transfer_pool>(
		std::make_shared<thread_pool_executor>(2)
	);
}

} // namespace test
} // namespace vitrio
