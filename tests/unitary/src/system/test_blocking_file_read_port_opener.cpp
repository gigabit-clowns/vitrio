// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/blocking_file_read_port_opener.hpp>

#include "../fixtures/file_read_port_contract.hpp"

#include <memory>

using namespace vitrio;

TEST_CASE(
	"the ports a blocking_file_read_port_opener opens keep the contract of a "
	"file_read_port",
	"[blocking_file_read_port_opener]"
)
{
	const blocking_file_read_port_opener ports;
	check_file_read_port_contract(
		"blocking_file_read_port_opener_contract.raw",
		[&ports]
		{
			return ports.open();
		}
	);
}
