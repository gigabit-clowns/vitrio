// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/blocking_file_read_queue.hpp>

#include "../fixtures/file_read_queue_contract.hpp"

#include <memory>

using namespace vitrio;

TEST_CASE(
	"blocking_file_read_queue keeps the contract of a file_read_queue",
	"[blocking_file_read_queue]"
)
{
	check_file_read_queue_contract(
		"blocking_file_read_queue_contract.raw",
		[]
		{
			return std::make_unique<blocking_file_read_queue>();
		}
	);
}
