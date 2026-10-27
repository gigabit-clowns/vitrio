// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_core_rank.hpp>

using namespace vitrio;
using namespace vitrio::zarr;
using nlohmann::json;

namespace
{

const std::vector<std::string> unnamed;

zarr_multiscale make_image(
	const std::vector<std::string> &types,
	const std::vector<std::vector<double>> &scales
)
{
	zarr_multiscale image;
	for (const auto &type : types)
	{
		image.axes.push_back({"a", type, std::string()});
	}
	for (const auto &scale : scales)
	{
		image.datasets.push_back({"level", scale});
	}

	return image;
}

} // anonymous namespace

TEST_CASE( "decide_core_rank tells a stack from a volume",
	"[zarr_core_rank]" )
{
	SECTION( "what vitrio wrote prevails" )
	{
		const auto image = make_image(
			{"space", "space", "space"}, {{1, 1, 1}});
		REQUIRE( decide_core_rank(
			3, json::parse(R"({"vitrio": {"core_rank": 2}})"), unnamed,
			&image) == 2 );
	}

	SECTION( "what vitrio could not have written is passed over" )
	{
		REQUIRE( decide_core_rank(
			3, json::parse(R"({"vitrio": {"core_rank": 4}})"), unnamed,
			nullptr) == 3 );
		REQUIRE( decide_core_rank(
			3, json::parse(R"({"vitrio": {"core_rank": 0}})"), unnamed,
			nullptr) == 3 );
	}

	SECTION( "a tomogram, every axis of space scaled alike" )
	{
		const auto image = make_image(
			{"space", "space", "space"}, {{10, 10, 10}, {20, 20, 20}});
		REQUIRE( decide_core_rank(3, json::object(), unnamed, &image) == 3 );
	}

	SECTION( "a tilt series, its first axis never scaled" )
	{
		const auto image = make_image(
			{"space", "space", "space"},
			{{1, 10, 10}, {1, 20, 20}, {1, 40, 40}});
		REQUIRE( decide_core_rank(3, json::object(), unnamed, &image) == 2 );
	}

	SECTION( "a single level says nothing of a stack" )
	{
		const auto image = make_image(
			{"space", "space", "space"}, {{1, 10, 10}});
		REQUIRE( decide_core_rank(3, json::object(), unnamed, &image) == 3 );
	}

	SECTION( "axes of time and channels lead as a stack" )
	{
		const auto image = make_image(
			{"time", "channel", "space", "space", "space"},
			{{1, 1, 1, 1, 1}});
		REQUIRE( decide_core_rank(5, json::object(), unnamed, &image) == 3 );

		const auto planes = make_image(
			{"channel", "space", "space"}, {{1, 1, 1}});
		REQUIRE( decide_core_rank(3, json::object(), unnamed, &planes) ==
			2 );
	}

	SECTION( "axes that do not match the array are passed over" )
	{
		const auto image = make_image({"space", "space"}, {{1, 1}});
		REQUIRE( decide_core_rank(3, json::object(), unnamed, &image) == 3 );
	}

	SECTION( "the names of the dimensions, without OME-Zarr" )
	{
		REQUIRE( decide_core_rank(
			3, json::object(), {"n", "y", "x"}, nullptr) == 2 );
		REQUIRE( decide_core_rank(
			4, json::object(), {"t", "z", "y", "x"}, nullptr) == 3 );
	}

	SECTION( "otherwise three axes at most" )
	{
		REQUIRE( decide_core_rank(1, json::object(), unnamed, nullptr) == 1 );
		REQUIRE( decide_core_rank(2, json::object(), unnamed, nullptr) == 2 );
		REQUIRE( decide_core_rank(3, json::object(), unnamed, nullptr) == 3 );
		REQUIRE( decide_core_rank(5, json::object(), unnamed, nullptr) == 3 );
		REQUIRE( decide_core_rank(
			3, json::object(), {"a", "b", "c"}, nullptr) == 3 );
	}
}
