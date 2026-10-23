// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <file_transfer_pool_implementation.hpp>

#include <vitrio/concurrency/thread_pool_executor.hpp>

#include <memory>

using namespace vitrio;

TEST_CASE(
	"a file_transfer_pool::implementation keeps its executor for whoever "
	"copies it",
	"[file_transfer_pool]"
)
{
	std::weak_ptr<file_transfer_executor> watched;
	std::shared_ptr<file_transfer_executor> kept;
	{
		const file_transfer_pool::implementation pool(
			std::make_shared<thread_pool_executor>(1)
		);
		kept = pool.get_executor();
		watched = kept;
	}

	CHECK_FALSE( watched.expired() );
	kept.reset();
	CHECK( watched.expired() );
}
