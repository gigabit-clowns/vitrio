// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/asynchronous_file_transfer_port_opener.hpp>

#include "../fixtures/file_transfer_port_contract.hpp"
#include "../fixtures/file_transfer_queue_contract.hpp"

#include <memory>

using namespace vitrio;

TEST_CASE(
	"asynchronous_file_transfer_port_opener tells the same every time whether "
	"the process can have its ports",
	"[asynchronous_file_transfer_port_opener]"
)
{
	const auto supported =
		asynchronous_file_transfer_port_opener::is_supported();
	CHECK(
		asynchronous_file_transfer_port_opener::is_supported() == supported
	);
}

TEST_CASE(
	"the ports an asynchronous_file_transfer_port_opener opens keep the "
	"contract of a file_transfer_port",
	"[asynchronous_file_transfer_port_opener]"
)
{
	if (!asynchronous_file_transfer_port_opener::is_supported())
	{
		SKIP( "The process can not have asynchronous ports here." );
	}

	// Ports with room for two reads, so that the cases queue more than they
	// hold.
	const asynchronous_file_transfer_port_opener ports(2);

	SECTION( "as a queue, when nothing wakes them" )
	{
		check_file_transfer_queue_contract(
			"asynchronous_file_transfer_port_opener_queue_contract.raw",
			[&ports]
			{
				return std::make_unique<port_file_transfer_queue>(ports.open());
			}
		);
	}

	SECTION( "as a port" )
	{
		check_file_transfer_port_contract(
			"asynchronous_file_transfer_port_opener_contract.raw",
			[&ports]
			{
				return ports.open();
			}
		);
	}
}
