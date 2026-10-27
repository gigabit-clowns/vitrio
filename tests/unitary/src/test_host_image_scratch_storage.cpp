// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/host_image_scratch_storage.hpp>

#include "fixtures/image_scratch_storage_contract.hpp"

#include <vitrio/image_scratch_storage.hpp>

#include <cstddef>
#include <stdexcept>

using namespace vitrio;

TEST_CASE(
	"a host image scratch storage keeps the contract of a scratch storage",
	"[host_image_scratch_storage]"
)
{
	check_image_scratch_storage_contract(
		"host_scratch_storage_contract",
		[](std::size_t size)
		{
			return create_host_image_scratch_storage(size);
		}
	);
}

TEST_CASE(
	"create_host_image_scratch_storage refuses a size of zero",
	"[host_image_scratch_storage]"
)
{
	REQUIRE_THROWS_AS(
		create_host_image_scratch_storage(0),
		std::invalid_argument
	);
}
