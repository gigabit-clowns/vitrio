// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/file_transfer_pool.hpp>

#include "mock/mock_file_transfer_operation.hpp"

#include <file_transfer_pool_implementation.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/concurrency/thread_pool_executor.hpp>
#include <vitrio/image_descriptor.hpp>
#include <vitrio/image_file_read_format_selector.hpp>
#include <vitrio/tests/assets.hpp>
#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/transfers.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

#include <trompeloeil.hpp>

using namespace vitrio;
using namespace vitrio::test;

TEST_CASE(
	"a file_transfer_pool needs an executor",
	"[file_transfer_pool]"
)
{
	CHECK_THROWS_AS( file_transfer_pool(nullptr), std::invalid_argument );
}

TEST_CASE(
	"a file_transfer_pool carries out operations on its threads",
	"[file_transfer_pool]"
)
{
	const file_transfer_pool transfers(
		std::make_shared<thread_pool_executor>(1)
	);
	const auto &executor = transfers.get_implementation().get_executor();
	REQUIRE( executor != nullptr );

	auto operation = std::make_unique<mock_file_transfer_operation>();
	operation->expectations.push_back(
		NAMED_REQUIRE_CALL(*operation, start(trompeloeil::_))
	);

	const auto done = std::make_shared<counting_completion>(1);
	executor->submit(std::move(operation), done);
	CHECK_NOTHROW( done->get() );
}

TEST_CASE(
	"a reader opened with a file_transfer_pool reads after the pool is gone",
	"[file_transfer_pool]"
)
{
	auto transfers = std::make_unique<file_transfer_pool>(
		std::make_shared<thread_pool_executor>(2)
	);
	const auto reader = image_file_read_format_selector::get_shared()->open(
		get_mrc_asset_path("EMD-3197.map"),
		*transfers
	);
	transfers.reset();

	const auto &descriptor = reader->get_descriptor();
	const auto extents = std::vector<std::size_t>(
		descriptor.get_extents().begin(),
		descriptor.get_extents().end()
	);
	auto destination = make_host_array<float>(
		extents,
		numerical_type::float32
	);
	CHECK_NOTHROW( read_now(*reader, destination, whole_of(extents)) );
}
