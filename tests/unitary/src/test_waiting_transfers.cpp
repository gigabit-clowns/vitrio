// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <waiting_transfers.hpp>

#include "mock/mock_image_reader.hpp"
#include "mock/mock_image_writer.hpp"

#include <vitrio/array/array.hpp>
#include <vitrio/array/numerical_type.hpp>
#include <vitrio/concurrency/completion_notifier.hpp>
#include <vitrio/exceptions/image_file_error.hpp>

#include <vitrio/tests/host_array.hpp>
#include <vitrio/tests/whole_region_plan.hpp>

#include <chrono>
#include <exception>
#include <memory>
#include <thread>

#include <trompeloeil.hpp>

using namespace vitrio;
using namespace vitrio::test;

namespace
{

// Lands a value in the first element of an array, and reports, a moment
// after it is started: a read still under way when the call that started it
// returns.
void land_late(
	array destination,
	std::shared_ptr<completion_notifier> done,
	bool failing
)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	if (failing)
	{
		done->notify_done(
			std::make_exception_ptr(image_file_error("from the read"))
		);
		return;
	}
	*reinterpret_cast<float*>(destination.get_data()) = 7.0f;
	done->notify_done();
}

} // anonymous namespace

TEST_CASE(
	"read_and_wait returns once the read has landed",
	"[waiting_transfers]"
)
{
	const mock_image_reader reader;
	auto destination = make_host_array<float>({4}, numerical_type::float32);
	std::thread landing;

	REQUIRE_CALL(
		reader,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_SIDE_EFFECT(
			landing = std::thread(land_late, _1.share(), _3, false)
		);

	read_and_wait(reader, destination.share(), whole_of({4}));
	const auto landed = get_values<float>(destination).front();
	landing.join();

	CHECK( landed == 7.0f );
}

TEST_CASE(
	"read_and_wait throws what the read reports",
	"[waiting_transfers]"
)
{
	const mock_image_reader reader;
	auto destination = make_host_array<float>({4}, numerical_type::float32);
	std::thread landing;

	REQUIRE_CALL(
		reader,
		read_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.LR_SIDE_EFFECT(
			landing = std::thread(land_late, _1.share(), _3, true)
		);

	CHECK_THROWS_AS(
		read_and_wait(reader, destination.share(), whole_of({4})),
		image_file_error
	);
	landing.join();
}

TEST_CASE(
	"write_and_wait and flush_and_wait throw what the writer reports",
	"[waiting_transfers]"
)
{
	mock_image_writer writer;
	const auto source = make_host_array<float>({4}, numerical_type::float32);

	REQUIRE_CALL(
		writer,
		write_async(trompeloeil::_, trompeloeil::_, trompeloeil::_)
	)
		.SIDE_EFFECT( _3->notify_done(
			std::make_exception_ptr(image_file_error("from the write"))
		) );
	REQUIRE_CALL(writer, flush_async(trompeloeil::_))
		.SIDE_EFFECT( _1->notify_done(
			std::make_exception_ptr(image_file_error("from the flush"))
		) );

	CHECK_THROWS_AS(
		write_and_wait(writer, source.share_const(), whole_of({4})),
		image_file_error
	);
	CHECK_THROWS_AS( flush_and_wait(writer), image_file_error );
}
