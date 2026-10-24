// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <system/environment_variable.hpp>

#include <cstdlib>
#include <string>

using namespace vitrio;

namespace
{

void set_variable(const char *name, const char *value)
{
#if defined(_WIN32)
	_putenv_s(name, value);
#else
	::setenv(name, value, 1);
#endif
}

void unset_variable(const char *name)
{
#if defined(_WIN32)
	_putenv_s(name, "");
#else
	::unsetenv(name);
#endif
}

} // anonymous namespace

TEST_CASE(
	"get_environment_variable gives the value of a variable that is set",
	"[environment_variable]"
)
{
	const char *const name = "VITRIO_TEST_ENVIRONMENT_VARIABLE";
	set_variable(name, "a value");

	const auto value = get_environment_variable(name);
	REQUIRE( value.has_value() );
	CHECK( *value == "a value" );

	unset_variable(name);
}

TEST_CASE(
	"get_environment_variable gives none for a variable that is not set",
	"[environment_variable]"
)
{
	const char *const name = "VITRIO_TEST_ENVIRONMENT_VARIABLE_UNSET";
	unset_variable(name);

	CHECK_FALSE( get_environment_variable(name).has_value() );
}
