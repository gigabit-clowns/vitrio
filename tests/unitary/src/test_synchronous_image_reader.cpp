// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include "mock/mock_synchronous_image_reader.hpp"

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/exceptions/image_file_error.hpp>

#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <memory>
#include <stdexcept>

#include <trompeloeil.hpp>

using namespace vitrio;
using namespace vitrio::test;

TEST_CASE(
	"a synchronous_image_reader refuses a null notifier",
	"[synchronous_image_reader]"
)
{
	const mock_synchronous_image_reader reader;
	auto destination = make_host_array<float>({2, 3}, numerical_type::float32);
	FORBID_CALL(reader, read(trompeloeil::_, trompeloeil::_));

	CHECK_THROWS_AS(
		reader.read_async(destination.share(), whole_of({2, 3}), nullptr),
		std::invalid_argument
	);
}

TEST_CASE(
	"a synchronous_image_reader reads and reports before returning",
	"[synchronous_image_reader]"
)
{
	const mock_synchronous_image_reader reader;
	auto destination = make_host_array<float>({2, 3}, numerical_type::float32);
	const auto done = std::make_shared<counting_completion>(1);

	REQUIRE_CALL(reader, read(trompeloeil::_, trompeloeil::_))
		.LR_WITH(
			_1.get_data() == destination.get_data() &&
			_2.get_region_count() == 1
		);

	reader.read_async(destination.share(), whole_of({2, 3}), done);

	CHECK( done->is_ready() );
	CHECK_NOTHROW( done->get() );
}

TEST_CASE(
	"a synchronous_image_reader reports what its read throws",
	"[synchronous_image_reader]"
)
{
	const mock_synchronous_image_reader reader;
	auto destination = make_host_array<float>({2, 3}, numerical_type::float32);
	const auto done = std::make_shared<counting_completion>(1);

	REQUIRE_CALL(reader, read(trompeloeil::_, trompeloeil::_))
		.THROW( image_file_error("from the read") );

	CHECK_NOTHROW(
		reader.read_async(destination.share(), whole_of({2, 3}), done)
	);

	REQUIRE( done->is_ready() );
	CHECK_THROWS_AS( done->get(), image_file_error );
}
