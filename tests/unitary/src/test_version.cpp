// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/version.hpp>

#include <sstream>

using namespace vitrio;

TEST_CASE(
	"version reports the components it was constructed from",
	"[version]"
)
{
	const version ver(1, 2, 3);

	CHECK(ver.get_major() == 1);
	CHECK(ver.get_minor() == 2);
	CHECK(ver.get_patch() == 3);
}

TEST_CASE(
	"version can be used in a constant expression",
	"[version]"
)
{
	constexpr version ver(1, 2, 3);

	static_assert(ver.get_major() == 1, "The major component is constant");
	static_assert(ver.get_minor() == 2, "The minor component is constant");
	static_assert(ver.get_patch() == 3, "The patch component is constant");
	static_assert(ver < version(1, 2, 4), "The comparison is constant");
}

TEST_CASE(
	"version compares equal when every component matches",
	"[version]"
)
{
	const version ver(1, 2, 3);

	CHECK(ver == version(1, 2, 3));
	CHECK_FALSE(ver != version(1, 2, 3));

	CHECK(ver != version(9, 2, 3));
	CHECK(ver != version(1, 9, 3));
	CHECK(ver != version(1, 2, 9));
	CHECK_FALSE(ver == version(1, 2, 9));
}

TEST_CASE(
	"version is ordered by major, then minor, then patch",
	"[version]"
)
{
	// The major component outweighs the other two.
	CHECK(version(1, 9, 9) < version(2, 0, 0));
	CHECK(version(2, 0, 0) > version(1, 9, 9));

	// The minor component outweighs the patch.
	CHECK(version(1, 2, 9) < version(1, 3, 0));
	CHECK(version(1, 3, 0) > version(1, 2, 9));

	CHECK(version(1, 2, 3) < version(1, 2, 4));
	CHECK(version(1, 2, 4) > version(1, 2, 3));
}

TEST_CASE(
	"version inclusive comparisons hold for equal versions",
	"[version]"
)
{
	const version ver(1, 2, 3);

	CHECK(ver <= version(1, 2, 3));
	CHECK(ver >= version(1, 2, 3));
	CHECK_FALSE(ver < version(1, 2, 3));
	CHECK_FALSE(ver > version(1, 2, 3));

	CHECK(ver <= version(1, 2, 4));
	CHECK_FALSE(ver >= version(1, 2, 4));
}

TEST_CASE(
	"version is written as its components separated by dots",
	"[version]"
)
{
	std::ostringstream text;

	text << version(1, 20, 300);

	CHECK(text.str() == "1.20.300");
}

TEST_CASE(
	"version is written to a wide stream",
	"[version]"
)
{
	std::wostringstream text;

	text << version(1, 20, 300);

	CHECK(text.str() == L"1.20.300");
}
