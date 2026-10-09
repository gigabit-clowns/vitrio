// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <vitrio/image_read_format_manager.hpp>

#include "fixtures/format_manager_fixture.hpp"
#include <vitrio/tests/scoped_path.hpp>
#include "mock/mock_image_reader.hpp"
#include "mock/mock_image_read_format.hpp"

#include <vitrio/exceptions/image_file_error.hpp>
#include <vitrio/exceptions/unsupported_operation_error.hpp>
#include <vitrio/image_probe.hpp>

#include <fstream>
#include <memory>
#include <string>
#include <trompeloeil.hpp>

using namespace vitrio;

namespace
{

auto names(const std::string &path)
{
	return Catch::Matchers::MessageMatches(
		Catch::Matchers::StartsWith(path + ": ")
	);
}

} // anonymous namespace

TEST_CASE( "an empty read manager recognizes nothing",
	"[image_read_format_manager]" )
{
	const image_read_format_manager manager;

	SECTION( "no format claims a file" )
	{
		REQUIRE( manager.get_most_suitable_format(
			image_probe("absent.mrc")) == nullptr );
	}

	SECTION( "opening a path no file is at reports it missing" )
	{
		REQUIRE_THROWS_MATCHES(
			manager.open("absent.mrc"),
			image_file_error,
			names("absent.mrc")
		);
	}

	SECTION( "opening a file that is there reports it unsupported" )
	{
		const scoped_path path("read_manager_unclaimed.bin");
		std::ofstream(path.get().c_str(), std::ios::binary).put('\0');

		REQUIRE_THROWS_MATCHES(
			manager.open(path.get()),
			unsupported_operation_error,
			names(path.get())
		);
	}
}

TEST_CASE_METHOD(
	read_format_manager_fixture,
	"the read manager picks the most suitable format",
	"[image_read_format_manager]"
)
{
	const auto &manager = *get_manager();
	const image_probe probe("absent.mrc");

	SECTION( "the only supporting format is chosen" )
	{
		const auto &only = add_format(image_format_suitability::normal);

		REQUIRE( manager.get_most_suitable_format(probe) == &only );
	}

	SECTION( "the highest priority wins" )
	{
		add_format(image_format_suitability::fallback);
		const auto &optimal = add_format(image_format_suitability::optimal);
		add_format(image_format_suitability::normal);

		REQUIRE( manager.get_most_suitable_format(probe) == &optimal );
	}

	SECTION( "a format reporting unsupported is never chosen" )
	{
		add_format(image_format_suitability::unsupported);
		const auto &accepts = add_format(image_format_suitability::fallback);

		REQUIRE( manager.get_most_suitable_format(probe) == &accepts );
	}

	SECTION( "every format declining leaves nothing suitable" )
	{
		add_format(image_format_suitability::unsupported);
		add_format(image_format_suitability::unsupported);

		REQUIRE( manager.get_most_suitable_format(probe) == nullptr );
		REQUIRE_THROWS_MATCHES(
			manager.open("absent.mrc"),
			image_file_error,
			names("absent.mrc")
		);
	}

	SECTION( "the chosen format opens a path that names no file" )
	{
		// A path is only a locator: a format may claim one no local file is
		// at, such as the address of a remote one.
		auto &only = add_format(image_format_suitability::normal);
		const auto reader = std::make_shared<mock_image_reader>();

		REQUIRE_CALL(only, open(ANY(const image_probe&)))
			.LR_WITH( _1.get_path() == "absent.mrc" )
			.RETURN(reader);

		REQUIRE( manager.open("absent.mrc") == reader );
	}
}

TEST_CASE( "the read manager refuses a null format",
	"[image_read_format_manager]" )
{
	image_read_format_manager manager;

	REQUIRE_FALSE( manager.register_format(nullptr) );
	REQUIRE( manager.register_format(
		std::make_unique<mock_image_read_format>()) );
}

TEST_CASE( "the read manager consults every registered format",
	"[image_read_format_manager]" )
{
	image_read_format_manager manager;

	auto first = std::make_unique<mock_image_read_format>();
	auto second = std::make_unique<mock_image_read_format>();

	REQUIRE_CALL(*first, get_suitability(ANY(const image_probe&)))
		.RETURN(image_format_suitability::unsupported);
	REQUIRE_CALL(*second, get_suitability(ANY(const image_probe&)))
		.RETURN(image_format_suitability::normal);
	ALLOW_CALL(*second, get_name()).RETURN(std::string("second"));

	const auto *expected = second.get();
	manager.register_format(std::move(first));
	manager.register_format(std::move(second));

	const auto *chosen = manager.get_most_suitable_format(
		image_probe("absent.mrc"));

	REQUIRE( chosen == expected );
}
