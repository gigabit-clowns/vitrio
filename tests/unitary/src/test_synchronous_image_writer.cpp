// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include "mock/mock_synchronous_image_writer.hpp"

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
	"a synchronous_image_writer refuses a null notifier",
	"[synchronous_image_writer]"
)
{
	mock_synchronous_image_writer writer;
	const auto source = make_host_array<float>({2, 3}, numerical_type::float32);
	FORBID_CALL(writer, write(trompeloeil::_, trompeloeil::_));
	FORBID_CALL(writer, flush());

	CHECK_THROWS_AS(
		writer.write_async(source.share_const(), whole_of({2, 3}), nullptr),
		std::invalid_argument
	);
	CHECK_THROWS_AS( writer.flush_async(nullptr), std::invalid_argument );
}

TEST_CASE(
	"a synchronous_image_writer writes and flushes, and reports before "
	"returning",
	"[synchronous_image_writer]"
)
{
	mock_synchronous_image_writer writer;
	const auto source = make_host_array<float>({2, 3}, numerical_type::float32);

	REQUIRE_CALL(writer, write(trompeloeil::_, trompeloeil::_))
		.LR_WITH(
			_1.get_data() == source.get_data() &&
			_2.get_region_count() == 1
		);
	REQUIRE_CALL(writer, flush());

	const auto written = std::make_shared<counting_completion>(1);
	writer.write_async(source.share_const(), whole_of({2, 3}), written);
	CHECK( written->is_ready() );
	CHECK_NOTHROW( written->get() );

	const auto flushed = std::make_shared<counting_completion>(1);
	writer.flush_async(flushed);
	CHECK( flushed->is_ready() );
	CHECK_NOTHROW( flushed->get() );
}

TEST_CASE(
	"a synchronous_image_writer reports what its write and flush throw",
	"[synchronous_image_writer]"
)
{
	mock_synchronous_image_writer writer;
	const auto source = make_host_array<float>({2, 3}, numerical_type::float32);

	REQUIRE_CALL(writer, write(trompeloeil::_, trompeloeil::_))
		.THROW( image_file_error("from the write") );
	REQUIRE_CALL(writer, flush())
		.THROW( image_file_error("from the flush") );

	const auto written = std::make_shared<counting_completion>(1);
	CHECK_NOTHROW(
		writer.write_async(source.share_const(), whole_of({2, 3}), written)
	);
	REQUIRE( written->is_ready() );
	CHECK_THROWS_AS( written->get(), image_file_error );

	const auto flushed = std::make_shared<counting_completion>(1);
	CHECK_NOTHROW( writer.flush_async(flushed) );
	REQUIRE( flushed->is_ready() );
	CHECK_THROWS_AS( flushed->get(), image_file_error );
}
