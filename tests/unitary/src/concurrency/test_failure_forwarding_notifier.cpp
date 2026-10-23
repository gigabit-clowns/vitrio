// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <concurrency/failure_forwarding_notifier.hpp>

#include <vitrio/concurrency/counting_completion.hpp>

#include <exception>
#include <memory>
#include <stdexcept>

using namespace vitrio;

TEST_CASE(
	"failure_forwarding_notifier refuses a null parent",
	"[failure_forwarding_notifier]"
)
{
	CHECK_THROWS_AS(
		failure_forwarding_notifier(nullptr),
		std::invalid_argument
	);
}

TEST_CASE(
	"failure_forwarding_notifier does not report a success",
	"[failure_forwarding_notifier]"
)
{
	const auto parent = std::make_shared<counting_completion>(1);
	failure_forwarding_notifier notifier(parent);

	notifier.notify_done();

	CHECK_FALSE( parent->is_ready() );
}

TEST_CASE(
	"failure_forwarding_notifier reports a failure",
	"[failure_forwarding_notifier]"
)
{
	const auto parent = std::make_shared<counting_completion>(1);
	failure_forwarding_notifier notifier(parent);

	notifier.notify_done(
		std::make_exception_ptr(std::runtime_error("failed"))
	);

	REQUIRE( parent->is_ready() );
	CHECK_THROWS_AS( parent->get(), std::runtime_error );
}
