// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/concurrency/counting_completion.hpp>
#include <vitrio/concurrency/counting_notifier.hpp>

#include <cstddef>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace vitrio;

namespace
{

std::exception_ptr make_error(const std::string &what)
{
	return std::make_exception_ptr(std::runtime_error(what));
}

std::string get_message(const counting_completion &completion)
{
	try
	{
		completion.get();
	}
	catch (const std::runtime_error &error)
	{
		return error.what();
	}
	return "";
}

} // anonymous namespace

TEST_CASE(
	"counting_notifier refuses a null parent",
	"[counting_notifier]"
)
{
	CHECK_THROWS_AS( counting_notifier(1, nullptr), std::invalid_argument );
}

TEST_CASE(
	"counting_notifier with a count of zero reports at once",
	"[counting_notifier]"
)
{
	const auto parent = std::make_shared<counting_completion>(1);

	const counting_notifier notifier(0, parent);

	CHECK( parent->is_ready() );
	CHECK_NOTHROW( parent->get() );
}

TEST_CASE(
	"counting_notifier reports once every unit has reported",
	"[counting_notifier]"
)
{
	const auto parent = std::make_shared<counting_completion>(2);
	counting_notifier notifier(3, parent);

	notifier.notify_done();
	notifier.notify_done();
	CHECK_FALSE( parent->is_ready() );

	notifier.notify_done();

	// One report to the parent, which still waits for another.
	CHECK_FALSE( parent->is_ready() );
	parent->notify_done();
	CHECK( parent->is_ready() );
	CHECK_NOTHROW( parent->get() );
}

TEST_CASE(
	"counting_notifier reports the first error",
	"[counting_notifier]"
)
{
	const auto parent = std::make_shared<counting_completion>(1);
	counting_notifier notifier(3, parent);

	notifier.notify_done(make_error("first"));
	notifier.notify_done();
	notifier.notify_done(make_error("second"));

	REQUIRE( parent->is_ready() );
	CHECK( get_message(*parent) == "first" );
}

TEST_CASE(
	"counting_notifier takes reports from many threads",
	"[counting_notifier]"
)
{
	constexpr std::size_t count = 64;
	const auto parent = std::make_shared<counting_completion>(1);
	counting_notifier notifier(count, parent);

	std::vector<std::thread> threads;
	for (std::size_t i = 0; i < count; ++i)
	{
		threads.emplace_back([&notifier] { notifier.notify_done(); });
	}
	for (auto &thread : threads)
	{
		thread.join();
	}

	CHECK( parent->is_ready() );
	CHECK_NOTHROW( parent->get() );
}
