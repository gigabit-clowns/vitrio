// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <logger.hpp>

#include "fixtures/captured_log.hpp"

using namespace vitrio;

TEST_CASE(
	"the library logs to one logger of its own",
	"[logger]"
)
{
	SECTION( "it is the same logger in every call" )
	{
		REQUIRE( &get_logger() == &get_logger() );
	}

	SECTION( "it is named after the library" )
	{
		REQUIRE( get_logger().name() == "vitrio" );
	}

	SECTION( "it is not the default logger of spdlog" )
	{
		REQUIRE( &get_logger() != spdlog::default_logger_raw() );
	}
}

TEST_CASE(
	"the log macros write to the logger of the library",
	"[logger]"
)
{
	const captured_log log;

	SECTION( "a warning, with its arguments formatted" )
	{
		VITRIO_LOG_WARN("The file holds {} odd fields.", 3);

		REQUIRE_THAT(
			log.get(),
			Catch::Matchers::ContainsSubstring("The file holds 3 odd fields.")
		);
		REQUIRE_THAT(
			log.get(),
			Catch::Matchers::ContainsSubstring("warning")
		);
	}

	SECTION( "an error" )
	{
		VITRIO_LOG_ERROR("The file was lost.");

		REQUIRE_THAT(
			log.get(),
			Catch::Matchers::ContainsSubstring("The file was lost.")
		);
		REQUIRE_THAT(
			log.get(),
			Catch::Matchers::ContainsSubstring("error")
		);
	}
}

TEST_CASE(
	"the logger discards what is less severe than it is set to report",
	"[logger]"
)
{
	const captured_log log;

	VITRIO_LOG_DEBUG("A detail.");

	REQUIRE( log.get().empty() );
}
