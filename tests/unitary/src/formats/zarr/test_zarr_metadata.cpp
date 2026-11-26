// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <formats/zarr/zarr_metadata.hpp>

#include <vitrio/array/numerical_type.hpp>
#include <vitrio/image_metadata_fields.hpp>

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

using namespace vitrio;
using namespace vitrio::zarr;
using namespace vitrio::image_metadata_fields;

namespace
{

using extents = std::vector<std::size_t>;

image_descriptor describe(const extents &shape, std::size_t core_rank)
{
	return image_descriptor(
		make_span(shape), core_rank, numerical_type::float32);
}

zarr_axis space(const std::string &name, const std::string &unit)
{
	return {name, "space", unit};
}

// An image of z, y and x, in a unit, whose levels are at the scales given.
zarr_multiscale make_image(
	const std::string &unit,
	const std::vector<std::vector<double>> &scales
)
{
	zarr_multiscale image;
	image.axes = {space("z", unit), space("y", unit), space("x", unit)};
	for (std::size_t level = 0; level < scales.size(); ++level)
	{
		image.datasets.push_back({std::to_string(level), scales[level]});
	}

	return image;
}

} // anonymous namespace

TEST_CASE(
	"the pixel sizes of a level of OME-Zarr are its scale in the unit of "
	"its axes",
	"[zarr_metadata]"
)
{
	SECTION( "of a volume, along its three axes" )
	{
		const auto metadata = derive_metadata(
			make_image("angstrom", {{3.0, 2.0, 1.5}}), 0,
			describe({8, 8, 8}, 3)
		);

		REQUIRE( get_field(metadata, pixel_size_x) == 1.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 2.0 );
		REQUIRE( get_field(metadata, pixel_size_z) == 3.0 );
	}

	SECTION( "of the level asked for, and not of another" )
	{
		const auto image =
			make_image("angstrom", {{10, 10, 10}, {20, 20, 20}});

		const auto metadata =
			derive_metadata(image, 1, describe({4, 4, 4}, 3));

		REQUIRE( get_field(metadata, pixel_size_x) == 20.0 );
		REQUIRE( get_field(metadata, pixel_size_z) == 20.0 );
	}

	// A tilt series: three axes of space, of which the first is a stack.
	SECTION( "of a stack of images, along the two axes of one" )
	{
		const auto metadata = derive_metadata(
			make_image("angstrom", {{1.0, 2.0, 2.0}}), 0,
			describe({5, 8, 8}, 2)
		);

		REQUIRE( get_field(metadata, pixel_size_x) == 2.0 );
		REQUIRE( get_field(metadata, pixel_size_y) == 2.0 );
		REQUIRE( find_field(metadata, pixel_size_z) == nullptr );
	}

	SECTION( "times the scale the image states for every level" )
	{
		auto image = make_image("angstrom", {{1.0, 2.0, 4.0}});
		image.scale = {10.0, 10.0, 0.5};

		const auto metadata =
			derive_metadata(image, 0, describe({8, 8, 8}, 3));

		REQUIRE( get_field(metadata, pixel_size_x) == 2.0 );
		REQUIRE( get_field(metadata, pixel_size_y) == 20.0 );
		REQUIRE( get_field(metadata, pixel_size_z) == 10.0 );
	}

	SECTION( "of an image of time, channels and space, along those of space" )
	{
		zarr_multiscale image;
		image.axes = {
			{"t", "time", "second"},
			{"c", "channel", ""},
			space("z", "nanometer"),
			space("y", "nanometer"),
			space("x", "nanometer")
		};
		image.datasets.push_back({"0", {0.1, 1.0, 5.0, 0.5, 0.25}});

		const auto metadata =
			derive_metadata(image, 0, describe({2, 3, 4, 5, 6}, 3));

		REQUIRE( get_field(metadata, pixel_size_x) == 2.5 );
		REQUIRE( get_field(metadata, pixel_size_y) == 5.0 );
		REQUIRE( get_field(metadata, pixel_size_z) == 50.0 );
		REQUIRE( metadata.get_entries().size() == 3 );
	}
}

TEST_CASE(
	"a length of OME-Zarr is so many angstroms",
	"[zarr_metadata]"
)
{
	const auto pixel_size_in = [] (const std::string &unit)
	{
		const auto metadata = derive_metadata(
			make_image(unit, {{1.0, 1.0, 2.0}}), 0, describe({8, 8, 8}, 3));
		const auto *found = find_field(metadata, pixel_size_x);
		return found ? *found : 0.0;
	};

	REQUIRE( pixel_size_in("angstrom") == 2.0 );
	REQUIRE( pixel_size_in("picometer") == 0.02 );
	REQUIRE( pixel_size_in("nanometer") == 20.0 );
	REQUIRE( pixel_size_in("micrometer") == 2.0e4 );
	REQUIRE( pixel_size_in("millimeter") == 2.0e7 );
	REQUIRE( pixel_size_in("centimeter") == 2.0e8 );
	REQUIRE( pixel_size_in("meter") == 2.0e10 );

	// What states a scale and no distance: no unit, one of no length and
	// one this format does not know.
	REQUIRE( pixel_size_in("") == 0.0 );
	REQUIRE( pixel_size_in("second") == 0.0 );
	REQUIRE( pixel_size_in("parsec") == 0.0 );
	REQUIRE( pixel_size_in("Angstrom") == 0.0 );
}

TEST_CASE(
	"a level of OME-Zarr states no pixel size where it states none",
	"[zarr_metadata]"
)
{
	const auto volume = describe({8, 8, 8}, 3);

	SECTION( "along an axis that is not of space" )
	{
		auto image = make_image("angstrom", {{3.0, 2.0, 1.5}});
		image.axes[0].type = "time";
		image.axes[2].type = "";

		const auto metadata = derive_metadata(image, 0, volume);

		REQUIRE( find_field(metadata, pixel_size_x) == nullptr );
		REQUIRE( get_field(metadata, pixel_size_y) == 2.0 );
		REQUIRE( find_field(metadata, pixel_size_z) == nullptr );
	}

	SECTION( "along an axis whose scale is no distance" )
	{
		const auto image = make_image("angstrom", {{
			0.0, -2.0, std::numeric_limits<double>::quiet_NaN()
		}});

		REQUIRE( derive_metadata(image, 0, volume).get_entries().empty() );
	}

	SECTION( "a level with no scale" )
	{
		const auto image = make_image("angstrom", {{}});

		REQUIRE( derive_metadata(image, 0, volume).get_entries().empty() );
	}

	SECTION( "a level with another number of scales than the array has axes" )
	{
		const auto image = make_image("angstrom", {{2.0, 2.0}});

		REQUIRE( derive_metadata(image, 0, volume).get_entries().empty() );
	}

	SECTION( "an image with another number of axes than the array has" )
	{
		auto image = make_image("angstrom", {{1.0, 2.0, 2.0}});
		image.axes.pop_back();

		REQUIRE( derive_metadata(image, 0, volume).get_entries().empty() );
	}

	SECTION( "a scale for every level that is not one per axis is passed "
		"over" )
	{
		auto image = make_image("angstrom", {{3.0, 2.0, 1.5}});
		image.scale = {10.0};

		const auto metadata = derive_metadata(image, 0, volume);

		REQUIRE( get_field(metadata, pixel_size_x) == 1.5 );
	}
}

TEST_CASE(
	"the pixel sizes given for an array are stated along its axes",
	"[zarr_metadata]"
)
{
	image_metadata metadata;
	set_field(metadata, pixel_size_x, 1.5);
	set_field(metadata, pixel_size_y, 2.0);
	set_field(metadata, pixel_size_z, 3.0);

	SECTION( "of a volume" )
	{
		REQUIRE( derive_pixel_sizes(metadata, describe({8, 8, 8}, 3)) ==
			std::vector<double>({3.0, 2.0, 1.5}) );
	}

	SECTION( "of a stack of volumes, none along the axis it stacks on" )
	{
		REQUIRE( derive_pixel_sizes(metadata, describe({4, 8, 8, 8}, 3)) ==
			std::vector<double>({0.0, 3.0, 2.0, 1.5}) );
	}

	SECTION( "of a stack of images, which has no z" )
	{
		REQUIRE( derive_pixel_sizes(metadata, describe({5, 8, 8}, 2)) ==
			std::vector<double>({0.0, 2.0, 1.5}) );
	}

	SECTION( "of an image" )
	{
		REQUIRE( derive_pixel_sizes(metadata, describe({8, 8}, 2)) ==
			std::vector<double>({2.0, 1.5}) );
	}

	SECTION( "none where none is given" )
	{
		REQUIRE( derive_pixel_sizes(image_metadata(), describe({8, 8}, 2)) ==
			std::vector<double>({0.0, 0.0}) );
	}

	SECTION( "none of what is no distance" )
	{
		image_metadata wrong;
		set_field(wrong, pixel_size_x, -1.5);
		set_field(wrong, pixel_size_y, 0.0);
		set_field(
			wrong, pixel_size_z, std::numeric_limits<double>::infinity());

		REQUIRE( derive_pixel_sizes(wrong, describe({8, 8, 8}, 3)) ==
			std::vector<double>({0.0, 0.0, 0.0}) );
	}

	SECTION( "and what another format states is nothing to it" )
	{
		image_metadata other;
		other.set("mrc.labels", "Created by something else");
		other.set("tiff.software", "something");

		REQUIRE( derive_pixel_sizes(other, describe({8, 8}, 2)) ==
			std::vector<double>({0.0, 0.0}) );
	}
}
