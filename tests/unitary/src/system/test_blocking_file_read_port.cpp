// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/blocking_file_read_port.hpp>

#include "../fixtures/file_read_port_contract.hpp"
#include "../fixtures/file_read_queue_contract.hpp"

#include <memory>

using namespace vitrio;

TEST_CASE(
	"a blocking_file_read_port nothing wakes keeps the contract of a "
	"file_read_queue",
	"[blocking_file_read_port]"
)
{
	check_file_read_queue_contract(
		"blocking_file_read_port_queue_contract.raw",
		[]
		{
			return std::make_unique<port_file_read_queue>(
				std::make_unique<blocking_file_read_port>()
			);
		}
	);
}

TEST_CASE(
	"blocking_file_read_port keeps the contract of a file_read_port",
	"[blocking_file_read_port]"
)
{
	check_file_read_port_contract(
		"blocking_file_read_port_contract.raw",
		[]
		{
			return std::make_unique<blocking_file_read_port>();
		}
	);
}
