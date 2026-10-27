// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/blocking_file_transfer_port.hpp>

#include "../fixtures/file_transfer_port_contract.hpp"
#include "../fixtures/file_transfer_queue_contract.hpp"

#include <memory>

using namespace vitrio;

TEST_CASE(
	"a blocking_file_transfer_port nothing wakes keeps the contract of a "
	"file_transfer_queue",
	"[blocking_file_transfer_port]"
)
{
	check_file_transfer_queue_contract(
		"blocking_file_transfer_port_queue_contract.raw",
		[]
		{
			return std::make_unique<port_file_transfer_queue>(
				std::make_unique<blocking_file_transfer_port>()
			);
		}
	);
}

TEST_CASE(
	"blocking_file_transfer_port keeps the contract of a file_transfer_port",
	"[blocking_file_transfer_port]"
)
{
	check_file_transfer_port_contract(
		"blocking_file_transfer_port_contract.raw",
		[]
		{
			return std::make_unique<blocking_file_transfer_port>();
		}
	);
}
