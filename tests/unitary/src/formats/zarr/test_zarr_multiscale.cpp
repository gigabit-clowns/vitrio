// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_multiscale.hpp>

#include <vitrio/exceptions/image_file_format_error.hpp>

using namespace vitrio;
using namespace vitrio::zarr;
using nlohmann::json;

namespace
{

// The attributes the CryoET Data Portal gives a tomogram, of OME-Zarr 0.4.
const char *const portal_tomogram = R"({"multiscales": [{
	"version": "0.4",
	"axes": [
		{"name": "z", "type": "space", "unit": "angstrom"},
		{"name": "y", "type": "space", "unit": "angstrom"},
		{"name": "x", "type": "space", "unit": "angstrom"}
	],
	"datasets": [
		{"path": "0", "coordinateTransformations": [
			{"type": "scale", "scale": [13.48, 13.48, 13.48]}]},
		{"path": "1", "coordinateTransformations": [
			{"type": "scale", "scale": [26.96, 26.96, 26.96]},
			{"type": "translation", "translation": [1, 1, 1]}]}
	]
}]})";

} // anonymous namespace

TEST_CASE( "parse_multiscale reads the first image of OME-Zarr",
	"[zarr_multiscale]" )
{
	SECTION( "version 0.4, its axes, levels and scales" )
	{
		const auto image =
			parse_multiscale(json::parse(portal_tomogram), "test");
		REQUIRE( image.is_initialized() );
		REQUIRE( image->axes.size() == 3 );
		REQUIRE( image->axes[0].name == "z" );
		REQUIRE( image->axes[0].type == "space" );
		REQUIRE( image->axes[2].unit == "angstrom" );
		REQUIRE( image->datasets.size() == 2 );
		REQUIRE( image->datasets[0].path == "0" );
		REQUIRE( image->datasets[1].scale ==
			std::vector<double>{26.96, 26.96, 26.96} );
	}

	SECTION( "version 0.5, under ome" )
	{
		const auto image = parse_multiscale(json::parse(R"({"ome": {
			"version": "0.5",
			"multiscales": [{"axes": [{"name": "y", "type": "space"},
				{"name": "x", "type": "space"}],
				"datasets": [{"path": "s0"}]}]
		}})"), "test");
		REQUIRE( image.is_initialized() );
		REQUIRE( image->axes.size() == 2 );
		REQUIRE( image->datasets[0].path == "s0" );
		REQUIRE( image->datasets[0].scale.empty() );
	}

	SECTION( "axes named alone, as before 0.4" )
	{
		const auto image = parse_multiscale(json::parse(R"({"multiscales":
			[{"axes": ["t", "c", "z", "y", "x"],
			"datasets": [{"path": "0"}]}]})"), "test");
		REQUIRE( image->axes[0].type == "time" );
		REQUIRE( image->axes[1].type == "channel" );
		REQUIRE( image->axes[4].type == "space" );
	}

	SECTION( "attributes that state no image" )
	{
		REQUIRE_FALSE(
			parse_multiscale(json::object(), "test").is_initialized() );
		REQUIRE_FALSE( parse_multiscale(
			json::parse(R"({"ome": {"version": "0.5"}})"), "test"
		).is_initialized() );
	}

	SECTION( "images that are not as OME-Zarr states are malformed" )
	{
		REQUIRE_THROWS_AS(
			parse_multiscale(json::parse(R"({"multiscales": []})"), "test"),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			parse_multiscale(json::parse(
				R"({"multiscales": [{"datasets": []}]})"), "test"),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			parse_multiscale(json::parse(
				R"({"multiscales": [{"datasets": [{"path": ""}]}]})"),
				"test"),
			image_file_format_error);
		REQUIRE_THROWS_AS(
			parse_multiscale(json::parse(R"({"multiscales": [{
				"datasets": [{"path": "0", "coordinateTransformations":
					[{"type": "scale", "scale": ["a"]}]}]}]})"), "test"),
			image_file_format_error);
	}
}
