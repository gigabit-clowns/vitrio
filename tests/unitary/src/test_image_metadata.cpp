// SPDX-License-Identifier: LGPL-2.1-or-later

#include <catch2/catch_test_macros.hpp>

#include <vitrio/image_metadata.hpp>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace vitrio;

namespace
{

std::vector<std::string> get_labels(const image_metadata &metadata)
{
	std::vector<std::string> result;
	for (const auto &entry : metadata.get_entries())
	{
		result.push_back(entry.first);
	}

	return result;
}

} // anonymous namespace

TEST_CASE(
	"image_metadata states nothing until something is set",
	"[image_metadata]"
)
{
	const image_metadata metadata;

	REQUIRE( metadata.get_entries().empty() );
	REQUIRE( metadata.find("pixel_size_x") == nullptr );
	REQUIRE( metadata.find("mrc.labels") == nullptr );
}

TEST_CASE(
	"image_metadata holds a value under the label it was set under",
	"[image_metadata]"
)
{
	image_metadata metadata;
	metadata.set("mrc.labels", "Created by vitrio");

	SECTION( "it is found" )
	{
		const auto *value = metadata.find("mrc.labels");

		REQUIRE( value != nullptr );
		REQUIRE( *value == image_metadata_value("Created by vitrio") );
	}

	SECTION( "it is got" )
	{
		REQUIRE(
			metadata.get("mrc.labels") ==
			image_metadata_value("Created by vitrio")
		);
	}

	SECTION( "a label that only starts as it does finds nothing" )
	{
		REQUIRE( metadata.find("mrc.label") == nullptr );
		REQUIRE( metadata.find("mrc.labels.0") == nullptr );
	}

	SECTION( "setting it again replaces the value, and may change its type" )
	{
		metadata.set("mrc.labels", 3);

		REQUIRE( metadata.get_entries().size() == 1 );
		REQUIRE( metadata.get("mrc.labels") == image_metadata_value(3) );
	}
}

TEST_CASE(
	"image_metadata refuses to get what is not stated",
	"[image_metadata]"
)
{
	image_metadata metadata;
	metadata.set("pixel_size_x", 0.83);

	REQUIRE_THROWS_AS( metadata.get("pixel_size_y"), std::out_of_range );
	REQUIRE_THROWS_AS( metadata.get("mrc.labels"), std::out_of_range );
	REQUIRE_THROWS_AS( metadata.get(""), std::out_of_range );
}

TEST_CASE(
	"image_metadata holds any type under a label with a prefix",
	"[image_metadata]"
)
{
	image_metadata metadata;
	metadata.set("application.frames", 40);
	metadata.set("application.dose", 1.25);
	metadata.set("application.session", "krios-3");

	REQUIRE( metadata.get("application.frames") == image_metadata_value(40) );
	REQUIRE( metadata.get("application.dose") == image_metadata_value(1.25) );
	REQUIRE(
		metadata.get("application.session") ==
		image_metadata_value("krios-3")
	);
}

TEST_CASE(
	"image_metadata refuses a label that is neither a standard field nor "
	"has a prefix",
	"[image_metadata]"
)
{
	image_metadata metadata;
	metadata.set("pixel_size_x", 0.83);

	SECTION( "an empty label" )
	{
		REQUIRE_THROWS_AS( metadata.set("", 1), std::invalid_argument );
	}

	SECTION( "a standard field misspelled" )
	{
		REQUIRE_THROWS_AS(
			metadata.set("pixelsize_x", 0.83),
			std::invalid_argument
		);
		REQUIRE_THROWS_AS(
			metadata.set("Pixel_Size_X", 0.83),
			std::invalid_argument
		);
	}

	SECTION( "a label of the caller's own without a prefix" )
	{
		REQUIRE_THROWS_AS(
			metadata.set("session", "krios-3"),
			std::invalid_argument
		);
	}

	// Whatever was refused, the metadata is as it was.
	REQUIRE( get_labels(metadata) ==
		std::vector<std::string>{"pixel_size_x"} );
}

TEST_CASE(
	"image_metadata holds a standard field as its type and no other",
	"[image_metadata]"
)
{
	image_metadata metadata;

	SECTION( "a real is held as it is" )
	{
		metadata.set("pixel_size_x", 0.83);

		REQUIRE( metadata.get("pixel_size_x") == image_metadata_value(0.83) );
	}

	SECTION( "an integer given for a real is held as that real" )
	{
		metadata.set("pixel_size_x", 2);

		const auto &value = metadata.get("pixel_size_x");
		REQUIRE( value.get_type() == image_metadata_value_type::real );
		REQUIRE( value.get_real() == 2.0 );
	}

	SECTION( "a text given for a real is refused" )
	{
		REQUIRE_THROWS_AS(
			metadata.set("pixel_size_x", "0.83"),
			std::invalid_argument
		);
		REQUIRE( metadata.get_entries().empty() );
	}

	SECTION( "a refused value leaves the one already held" )
	{
		metadata.set("pixel_size_x", 0.83);

		REQUIRE_THROWS_AS(
			metadata.set("pixel_size_x", "1.06"),
			std::invalid_argument
		);
		REQUIRE( metadata.get("pixel_size_x") == image_metadata_value(0.83) );
	}
}

TEST_CASE(
	"image_metadata does not convert under a label with a prefix",
	"[image_metadata]"
)
{
	image_metadata metadata;
	metadata.set("application.pixel_size_x", 2);

	REQUIRE(
		metadata.get("application.pixel_size_x").get_type() ==
		image_metadata_value_type::integer
	);
}

TEST_CASE(
	"image_metadata stops stating what is erased",
	"[image_metadata]"
)
{
	image_metadata metadata;
	metadata.set("pixel_size_x", 0.83);
	metadata.set("pixel_size_y", 0.83);

	SECTION( "what was stated is gone, and the rest stays" )
	{
		REQUIRE( metadata.erase("pixel_size_x") );

		REQUIRE( metadata.find("pixel_size_x") == nullptr );
		REQUIRE( get_labels(metadata) ==
			std::vector<std::string>{"pixel_size_y"} );
	}

	SECTION( "erasing what was not stated changes nothing" )
	{
		REQUIRE_FALSE( metadata.erase("pixel_size_z") );
		REQUIRE_FALSE( metadata.erase("not a label") );

		REQUIRE( metadata.get_entries().size() == 2 );
	}

	SECTION( "what was erased may be stated again" )
	{
		REQUIRE( metadata.erase("pixel_size_x") );
		metadata.set("pixel_size_x", 1.06);

		REQUIRE( metadata.get("pixel_size_x") == image_metadata_value(1.06) );
	}
}

TEST_CASE(
	"image_metadata lists its entries sorted by label",
	"[image_metadata]"
)
{
	image_metadata metadata;
	metadata.set("tiff.software", "vitrio");
	metadata.set("pixel_size_y", 0.83);
	metadata.set("mrc.labels", "Created by vitrio");
	metadata.set("pixel_size_x", 0.83);

	const std::vector<std::string> expected = {
		"mrc.labels", "pixel_size_x", "pixel_size_y", "tiff.software"
	};

	REQUIRE( get_labels(metadata) == expected );
	REQUIRE(
		metadata.get_entries()[3].second == image_metadata_value("vitrio")
	);
}

TEST_CASE(
	"image_metadata compares by what it states",
	"[image_metadata]"
)
{
	image_metadata metadata;
	metadata.set("pixel_size_x", 0.83);
	metadata.set("mrc.labels", "Created by vitrio");

	SECTION( "the order things were stated in does not matter" )
	{
		image_metadata other;
		other.set("mrc.labels", "Created by vitrio");
		other.set("pixel_size_x", 0.83);

		REQUIRE( metadata == other );
		REQUIRE_FALSE( metadata != other );
	}

	SECTION( "another value makes it another" )
	{
		image_metadata other(metadata);
		other.set("pixel_size_x", 1.06);

		REQUIRE( metadata != other );
	}

	SECTION( "one thing more or less makes it another" )
	{
		image_metadata other(metadata);
		other.erase("mrc.labels");

		REQUIRE( metadata != other );
		REQUIRE( other != image_metadata() );
	}

	SECTION( "metadata that states nothing equals any other that does" )
	{
		REQUIRE( image_metadata() == image_metadata() );
	}
}

TEST_CASE( "image_metadata has value semantics", "[image_metadata]" )
{
	image_metadata metadata;
	metadata.set("pixel_size_x", 0.83);

	SECTION( "a copy equals its source and changes apart from it" )
	{
		image_metadata copy(metadata);
		REQUIRE( copy == metadata );

		copy.set("pixel_size_x", 1.06);

		REQUIRE( metadata.get("pixel_size_x") == image_metadata_value(0.83) );
	}

	SECTION( "a moved one states what its source stated" )
	{
		image_metadata source(metadata);
		const image_metadata moved(std::move(source));

		REQUIRE( moved == metadata );
	}

	SECTION( "an assignment replaces everything" )
	{
		image_metadata assigned;
		assigned.set("mrc.labels", "Created by vitrio");
		assigned = metadata;

		REQUIRE( assigned == metadata );
	}
}
